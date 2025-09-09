classdef(ConstructOnLoad)ConnectionInterruptedEventData<event.EventData
	%断线重连或连接失败时抛出异常事件
	properties
		Exception
	end
	methods
		function obj = ConnectionInterruptedEventData(Exception)
			obj.Exception = Exception;
		end
	end
end