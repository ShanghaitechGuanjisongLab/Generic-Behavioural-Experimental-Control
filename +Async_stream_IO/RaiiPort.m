classdef RaiiPort<handle
	%随对象生命周期自动释放的端口。此对象生命周期结束时自动释放端口。
	properties(SetAccess=immutable)
		AsyncStream
		Port uint8
	end
	methods
		function obj = RaiiPort(AsyncStream,Port)
			%# 语法
			% ```
			% obj=Async_stream_IO.RaiiPort(AsyncStream);
			% %自动分配端口
			%
			% obj=Async_stream_IO.RaiiPort(AsyncStream,Port);
			% %手动指定端口
			% ```
			%# 输入参数
			% AsyncStream(1,1)Async_stream_IO.IAsyncStream: 异步流对象
			% Port(1,1)uint8: 端口号
			obj.AsyncStream = AsyncStream;
			if nargin>1
				obj.Port = Port;
			else
				obj.Port=AsyncStream.AllocatePort;
			end
		end
		function delete(obj)
			%释放端口
			obj.AsyncStream.ReleasePort(obj.Port);
		end
	end
end