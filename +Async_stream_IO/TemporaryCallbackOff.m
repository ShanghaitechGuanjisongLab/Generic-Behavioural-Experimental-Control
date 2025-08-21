classdef TemporaryCallbackOff<handle
	%构造此对象以临时关闭串口回调。此对象析构时恢复构造时的回调状态。
	properties
		Serial
		Callback
	end
	methods
		function obj = TemporaryCallbackOff(Serial)
			obj.Serial=Serial;
			if Serial.BytesAvailableFcnMode=="off"
				obj.Callback={'off'};
			else
				obj.Callback={Serial.BytesAvailableFcnMode,Serial.BytesAvailableFcnCount,Serial.BytesAvailableFcn};
				Serial.configureCallback('off');
			end
		end
		function delete(obj)
			obj.Serial.configureCallback(obj.Callback{:});
		end
	end
end