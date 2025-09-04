classdef TemporaryCallbackOff<handle
	%构造此对象以临时关闭串口回调。此对象析构时恢复构造时的回调状态。
	properties(SetAccess=immutable)
		AsyncStream
		InterruptEnabled
	end
	methods
		function obj = TemporaryCallbackOff(AsyncStream)
			obj.AsyncStream = AsyncStream;
			obj.InterruptEnabled = AsyncStream.InterruptEnabled;
			obj.AsyncStream.InterruptEnabled = false;
		end
		function delete(obj)
			obj.AsyncStream.InterruptEnabled = obj.InterruptEnabled;
		end
	end
end