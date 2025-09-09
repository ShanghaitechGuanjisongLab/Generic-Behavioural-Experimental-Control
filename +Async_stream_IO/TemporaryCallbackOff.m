classdef TemporaryCallbackOff<handle
	%构造此对象以临时关闭串口回调。此对象析构时恢复构造时的回调状态。
	properties(SetAccess=immutable)
		AsyncStream
		InterruptEnabled
	end
	methods
		function obj = TemporaryCallbackOff(AsyncStream)
			%# 语法
			% ```
			% obj = Async_stream_IO.TemporaryCallbackOff(AsyncStream);
			% ```
			%# 输入参数
			% AsyncStream(1,1)Async_stream_IO.IAsyncStream，实现了IAsyncStream接口的异步流对象
			obj.AsyncStream = AsyncStream;
			obj.InterruptEnabled = AsyncStream.InterruptEnabled;
			obj.AsyncStream.InterruptEnabled = false;
		end
		function delete(obj)
			obj.AsyncStream.InterruptEnabled = obj.InterruptEnabled;
		end
	end
end