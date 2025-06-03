classdef Server<handle
	properties(SetAccess=protected,Transient)
		%此属性只能用Initialize方法修改
		AsyncStream Async_stream_IO.IAsyncStream
	end
	properties(Access=protected,Transient)
		InitializeIsReadyLocalPort
		PointerSize
		Experiments
		NewProcess
	end
	properties(SetAccess=immutable)
		Name
		%串口长时间闲置时会自动关闭。
		SerialCountdown
	end
	methods(Access=protected)
		function SetPointerSize(obj,PointerSize)
			obj.PointerSize=PointerSize;
		end
		function ProcessFinished(obj,Pointer)
			if obj.Experiments.isKey(Pointer)
				obj.Experiments(Pointer).ExperimentFinished;
			end
		end
		function ProcessStarted(obj,Pointer)
			obj.NewProcess.Handle=Pointer;
		end
		function InitializeSuccess(obj)
			obj.AsyncStream.Listen(@(Exception)Gbec.Exception.Server_runtime_exception.Throw(Exception),Gbec.UID.PortC_Exception);
			obj.AsyncStream.Listen(@obj.ProcessFinished,Gbec.UID.PortC_ProcessFinished);
			obj.AsyncStream.Listen(@obj.ProcessStarted,Gbec.UID.PortC_ProcessStarted);

			obj.AsyncStream.RemoteInvoke(Gbec.UID.PortA_PointerSize,@obj.SetPointerSize);
			%该方法会随机占用端口，因此必须放在所有Listen之后。

			fprintf('%s %s 服务器初始化成功\n',datetime,obj.Name);
			obj.SerialCountdown.start;
		end
		function InitializeIsReadyCallbackA(obj,IsReady,Timer,varargin)
			Timer.stop;
			if IsReady
				obj.InitializeSuccess;
			else
				obj.AsyncStream=Async_stream_IO.AsyncSerialStream(varargin{:});
				obj.AsyncStream.RemoteInvoke(Gbec.UID.PortA_IsReady,@(IsReady)obj.InitializeIsReadyCallbackB(IsReady,Timer));
				Timer.start;
			end
		end
		function InitializeIsReadyCallbackB(obj,IsReady,Timer)
			Timer.stop;
			if IsReady
				obj.InitializeSuccess;
			else
				obj.SerialCountdown.start;
				Gbec.Exception.Serial_handshake_failed.Throw(sprintf('%s %s',datetime,obj.Name));
			end
		end
		function InitializeIsReadyTimeout(obj,~,~)
			obj.AsyncStream.ReleasePort(obj.InitializeIsReadyLocalPort);
			obj.SerialCountdown.start;
			Gbec.Exception.Server_initialize_timeout.Throw(sprintf('%s %s',datetime,obj.Name));
		end
	end
	methods
		function obj=Server(Name,SerialCountdown)
			arguments
				Name
				SerialCountdown=minutes(3)
			end
			obj.Name=Name;
			obj.SerialCountdown=MATLAB.Lang.Owner(timer(StartDelay=seconds(SerialCountdown),TimerFcn=@(~,~)delete(obj.AsyncStream)));
			obj.Experiments=dictionary;
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
			%
			% obj.Initialize(Timeout,___);
			% %与上述任意语法组合使用，额外指定响应超时
			% ```
			%# 输入参数
			% Timeout(1,1)duration=seconds(1)，服务器响应超时。超过这个时间未响视为初始化失败。
			% AsyncStream(1,1)Async_stream_IO.IAsyncStream，自定义底层异步流
			%See also Async_stream_IO.AsyncSerialStream Async_stream_IO.IAsyncStream
			obj.SerialCountdown.stop;
			if isduration(varargin{1})
				Timeout=seconds(varargin{1});
				varargin(1)=[];
			else
				Timeout=1;
			end
			Timer=MATLAB.Lang.Owner(timer(StartDelay=Timeout,TimerFcn=@obj.InitializeIsReadyTimeout));
			if isa(varargin{1},'Async_stream_IO.IAsyncStream')
				obj.AsyncStream=varargin{1};
				Callback=@(IsReady)obj.InitializeIsReadyCallbackB(IsReady,Timer);
			elseif isempty(obj.AsyncStream)||~isvalid(obj.AsyncStream)||~obj.AsyncStream.CheckArguments(varargin{:})
				obj.AsyncStream=Async_stream_IO.AsyncSerialStream(varargin{:});
				Callback=@(IsReady)obj.InitializeIsReadyCallbackB(IsReady,Timer);
			else
				Callback=@(IsReady)obj.InitializeIsReadyCallbackA(IsReady,Timer,varargin{:});
			end
			obj.AsyncStream.BindFunctionToPort(@(~)randi(intmax('uint32'),'uint32'),Gbec.UID.PortC_RandomSeed);
			obj.InitializeIsReadyLocalPort=obj.AsyncStream.RemoteInvoke(Gbec.UID.PortA_IsReady,Callback);
			Timer.start;
		end
		function Process=CreateProcess(obj,ClassID,Repeat)
			%创建并启动具有指定ClassID的进程，可选指定重复次数
			arguments
				obj
				ClassID
				Repeat=1
			end
			Process=Gbec.Process(obj,[]);
			obj.AsyncStream.Receive(@(Handle)CreateProcessSuccess(Process,Handle),Gbec.UID.PortC_ProcessStarted);
			if Repeat==1
				obj.AsyncStream.Send(ClassID,Gbec.UID.PortA_CreateProcess);
			else
				obj.AsyncStream.Send([uint8(ClassID),Repeat],Gbec.UID.PortA_CreateProcess);
			end
		end
	end
end