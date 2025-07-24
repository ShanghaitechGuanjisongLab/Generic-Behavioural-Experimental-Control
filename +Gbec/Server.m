classdef Server<handle
	properties(SetAccess=protected,Transient)
		%此属性只能用Initialize方法修改
		AsyncStream Async_stream_IO.IAsyncStream
	end
	properties(Access=protected,Transient)
		PointerSize
	end
	properties(SetAccess=immutable)
		Name
		%串口长时间闲置时会自动关闭。
		SerialCountdown
	end
	methods
		function obj=Server(Name,SerialCountdown)
			arguments
				Name
				SerialCountdown=minutes(3)
			end
			obj.Name=Name;
			obj.SerialCountdown=MATLAB.Lang.Owner(timer(StartDelay=seconds(SerialCountdown),TimerFcn=@(~,~)delete(obj.AsyncStream)));
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
			else
				obj.AsyncStream=Async_stream_IO.AsyncSerialStream(varargin{:});
			end
			obj.PointerSize=obj.AsyncStream.SyncInvoke(uint8(Gbec.UID.IsReady));
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