classdef AsyncSerialStream<handle
	properties
		Serial
	end
	methods
		function obj = AsyncSerialStream(Port,BaudRate)
			arguments
				Port
				BaudRate=9600
			end
			obj.Serial=serialport(Port,BaudRate);
		end

		function outputArg = method1(obj,inputArg)
			%METHOD1 此处显示有关此方法的摘要
			%   此处显示详细说明
			outputArg = obj.Property1 + inputArg;
		end
	end
end