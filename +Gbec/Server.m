classdef Server<handle
	properties(SetAccess=protected,Transient)
		%此属性只能用Initialize方法修改
		AsyncStream Async_stream_IO.IAsyncStream
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
			disp(obj.Name + "：已释放关联的流");
		end
		function ConnectionInterruptedHandler(obj,EventData)
			Gbec.Exception.Server_connection_interrupted.Throw(sprintf('%s %s',obj.Name,formattedDisplayText(EventData)));
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
			arguments
				SerialCountdown=minutes(3)
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
			obj.SerialCountdown.stop;
			if isa(varargin{1},'Async_stream_IO.IAsyncStream')
				obj.AsyncStream=varargin{1};
			elseif obj.AsyncStream.isvalid&&obj.AsyncStream.CheckArguments(varargin{:})
				obj.SerialCountdown.start;
				return;
			else
				obj.AsyncStream=Async_stream_IO.AsyncSerialStream(varargin{:});
			end
			while(~obj.AsyncStream.SyncInvoke(uint8(Gbec.UID.PortA_IsReady)))
			end
			obj.PointerSize=obj.AsyncStream.SyncInvoke(uint8(Gbec.UID.PortA_PointerSize));
			obj.PointerType="uint"+string(obj.PointerSize*8);
			obj.AsyncStream.AsyncInvoke(uint8(Gbec.UID.PortA_RandomSeed),randi([0,intmax('uint32')],'uint32'));
			WeakReference=matlab.lang.WeakReference(obj);
			obj.AsyncStream.BindFunctionToPort(@(Arguments)WeakReference.Handle.ProcessForward(Arguments,"ProcessFinished_"),Gbec.UID.PortC_ProcessFinished);
			obj.AsyncStream.BindFunctionToPort(@(Arguments)WeakReference.Handle.ProcessForward(Arguments,"Signal_"),Gbec.UID.PortC_Signal);
			obj.AsyncStream.BindFunctionToPort(@(Arguments)WeakReference.Handle.ProcessForward(Arguments,"TrialStart_"),Gbec.UID.PortC_TrialStart);
			obj.AsyncStream.BindFunctionToPort(@(Arguments)Gbec.UID(Arguments).Throw,Gbec.UID.PortC_Exception);
			if ismissing(obj.Name)
				obj.Name=formattedDisplayText(varargin);
			end
			obj.ConnectionInterruptedListener=event.listener(obj.AsyncStream,'ConnectionInterrupted',@(~,EventData)WeakReference.Handle.ConnectionInterruptedHandler(EventData));
			obj.SerialCountdown.start;
		end
		function RefreshAllProcesses(obj)
			%刷新AllProcesses属性
			%此方法会清除AllProcesses属性并重新获取当前所有进程。通常不需要调用此方法，因为Server会自动维护AllProcesses属性。调用此方法后，所有之前的Process对象不会再收到消息；要继续接收消息，必须重新获取Process对象。
			Port=obj.AsyncStream.AllocatePort;
			obj.AsyncStream.Send(Port,Gbec.UID.PortA_AllProcesses);
			MessageSize=obj.AsyncStream.Listen(Port);
			for P=0:obj.PointerSize:MessageSize-1
				Pointer=obj.AsyncStream.Read(obj.PointerType);
				if~obj.AllProcesses.isKey(Pointer)
					obj.AllProcesses(Pointer)=matlab.lang.WeakReference;
				end
			end
			obj.FeedDogIfActive();
		end
	end
end