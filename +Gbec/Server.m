classdef Server<handle
	%控制服务器串口连接
	properties(SetAccess=protected,Transient)
		%此属性只能用Initialize方法修改
		AsyncStream
	end
	properties(SetAccess=?Gbec.Process)
		%所有进程
		AllProcesses dictionary
	end
	properties(SetAccess=immutable)
		%串口长时间闲置时会自动关闭。
		SerialCountdown
	end
	properties(Access=?Gbec.CountdownExempt_)
		CountdownExemptLeft=0;
	end
	properties(SetAccess=protected,GetAccess=?Gbec.Process)
		PointerSize
		PointerType
	end
	properties
		%Server的名称，可以任意设置，在输出消息时作为标识符
		Name(1,1)string=missing
	end
	properties(Access=protected)
		ConnectionInterruptedListener
		ConnectionResetListener
		oSerialTimeout=1
	end
	properties(Dependent)
		%串口读写超时时间，默认为1秒
		SerialTimeout(1,1)duration
	end
	methods(Access=protected)
		function ProcessForward(obj,Arguments,Method)
			Process=obj.AllProcesses(typecast(Arguments(1:obj.PointerSize),obj.PointerType)).Handle;
			if isvalid(Process)
				if numel(Arguments)>obj.PointerSize
					Process.(Method)(Arguments(obj.PointerSize+1:end));
				else
					Process.(Method)();
				end
			end
		end
		function ReleaseStream(obj)
			delete(obj.AsyncStream);
			disp(newline+obj.Name + "：已释放关联的流");
		end
		function ConnectionInterruptedHandler(obj,EventData)
			Gbec.Exception.Server_connection_interrupted.Throw(sprintf('%s %s',obj.Name,formattedDisplayText(EventData)));
		end
		function ConnectionResetHandler(obj)
			obj.AsyncStream.Listen(Gbec.UID.PortC_ImReady);
			obj.AsyncStream.AsyncInvoke(Gbec.UID.PortA_RandomSeed,randi([0,intmax('uint32')],'uint32'));
			if obj.AllProcesses.numEntries
				arrayfun(@(P)P.Handle.ConnectionReset_(),obj.AllProcesses.values);
			end
		end
	end
	methods(Access=?Gbec.Process)
		function FeedDogIfActive(obj)
			if obj.SerialCountdown.Running=="on"
				obj.SerialCountdown.stop;
				obj.SerialCountdown.start;
			end
		end
	end
	methods
		function obj=Server(SerialCountdown)
			%# 语法
			% ```
			% obj = Gbec.Server;
			% % 创建一个Server对象，使用默认的串口闲置关闭时间
			%
			% obj = Gbec.Server(SerialCountdown);
			% % 创建一个Server对象，使用指定的串口闲置关闭时间
			% ```
			%# 输入参数
			% SerialCountdown(1,1)duration=minutes(4)，串口闲置多长时间后自动关闭
			arguments
				SerialCountdown=minutes(4)
			end
			disp(['通用行为实验控制器' Gbec.Version().Me ' by 张天夫']);
			WeakReference=matlab.lang.WeakReference(obj);
			obj.SerialCountdown=timer(StartDelay=seconds(SerialCountdown),TimerFcn=@(~,~)WeakReference.Handle.ReleaseStream());
			obj.SerialCountdown.start;
		end
		function Initialize(obj,varargin)
			%初始化异步流服务器
			%# 语法
			% ```
			% obj.Initialize(AsyncStream);
			% %直接设置底层AsyncStream并初始化
			%
			% obj.Initialize(varargin);
			% %使用指定参数构造一个Async_stream_IO.AsyncSerialStream并初始化
			% ```
			%# 输入参数
			% AsyncStream(1,1)Async_stream_IO.IAsyncStream，自定义底层异步流
			%See also Async_stream_IO.AsyncSerialStream Async_stream_IO.IAsyncStream
			obj.FeedDogIfActive;
			if isa(varargin{1},'Async_stream_IO.IAsyncStream')
				obj.AsyncStream=varargin{1};
			else
				HasOld=~isempty(obj.AsyncStream)&&obj.AsyncStream.isvalid&&obj.AsyncStream.CheckArguments(varargin{:});
				if HasOld
					try
						obj.AsyncStream.Serial.NumBytesAvailable;
					catch ME
						if ME.identifier=="transportlib:transport:invalidConnectionState"
							HasOld=false;
						else
							ME.rethrow;
						end
					end
				end
				if~HasOld
					obj.AllProcesses=dictionary;
					obj.AsyncStream=Async_stream_IO.AsyncSerialStream(varargin{:});
					obj.AsyncStream.Serial.Timeout=obj.oSerialTimeout;
					obj.AsyncStream.Listen(Gbec.UID.PortC_ImReady);
				end
			end
			WeakReference=matlab.lang.WeakReference(obj);
			obj.AsyncStream.BindFunctionToPort(@(Arguments)WeakReference.Handle.ProcessForward(Arguments,"ProcessFinished_"),Gbec.UID.PortC_ProcessFinished);
			obj.AsyncStream.BindFunctionToPort(@(Arguments)WeakReference.Handle.ProcessForward(Arguments,"Signal_"),Gbec.UID.PortC_Signal);
			obj.AsyncStream.BindFunctionToPort(@(Arguments)WeakReference.Handle.ProcessForward(Arguments,"TrialStart_"),Gbec.UID.PortC_TrialStart);
			obj.AsyncStream.BindFunctionToPort(@(Arguments)Gbec.UID(Arguments).Throw,Gbec.UID.PortC_Exception);
			obj.AsyncStream.BindFunctionToPort(@disp,Gbec.UID.PortC_Debug);
			if ismissing(obj.Name)
				obj.Name=erase(formattedDisplayText(varargin{1}),newline);
			end
			obj.ConnectionInterruptedListener=event.listener(obj.AsyncStream,'ConnectionInterrupted',@(~,EventData)WeakReference.Handle.ConnectionInterruptedHandler(EventData));
			obj.PointerSize=obj.AsyncStream.SyncInvoke(Gbec.UID.PortA_PointerSize);
			obj.PointerType="uint"+string(obj.PointerSize*8);
			obj.AsyncStream.AsyncInvoke(Gbec.UID.PortA_RandomSeed,randi([0,intmax('uint32')],'uint32'));
			obj.ConnectionResetListener=event.listener(obj.AsyncStream,'ConnectionReset',@(~,~)WeakReference.Handle.ConnectionResetHandler());
		end
		function RefreshAllProcesses(obj)
			%刷新AllProcesses属性
			%此方法会清除AllProcesses属性并重新获取当前所有进程。通常不需要调用此方法，因为Server会自动维护AllProcesses属性。调用此方法后，所有之前的Process对象不会再收到消息；要继续接收消息，必须重新获取Process对象。
			Port=obj.AsyncStream.AllocatePort;
			TCO=Async_stream_IO.TemporaryCallbackOff(obj.AsyncStream);
			obj.AsyncStream.Send(Port,Gbec.UID.PortA_AllProcesses);
			NewDict=dictionary;
			for P=0:int8(obj.PointerSize):int8(obj.AsyncStream.Listen(Port))-1
				Pointer=obj.AsyncStream.Read(obj.PointerType);
				if obj.AllProcesses.isKey(Pointer)
					NewDict(Pointer)=obj.AllProcesses(Pointer);
				else
					NewDict(Pointer)=matlab.lang.WeakReference;
				end
			end
			obj.AllProcesses=NewDict;
			obj.FeedDogIfActive();
		end
		function delete(obj)
			warning off MATLAB:timer:deleterunning;
			delete(obj.SerialCountdown);
			delete(obj.ConnectionInterruptedListener);
		end
		function ST=get.SerialTimeout(obj)
			ST=seconds(obj.oSerialTimeout);
		end
		function set.SerialTimeout(obj,Value)
			if isduration(Value)
				Value=seconds(Value);
			end
			obj.oSerialTimeout=Value;
			if~isempty(obj.AsyncStream)&&obj.AsyncStream.isvalid&&obj.AsyncStream.Serial.isvalid
				obj.AsyncStream.Serial.Timeout=Value;
			end
		end
	end
end