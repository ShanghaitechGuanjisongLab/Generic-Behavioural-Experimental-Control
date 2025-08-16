classdef Formal<Gbec.Process
	%正规实验进程
	properties
		SessionID(1,1)Gbec.UID
		Mouse
		DateTime
	end
	methods
		function obj = Formal(Server)
			obj@Gbec.Process(Server);
		end
		function outputArg = method1(obj,inputArg)
			%METHOD1 此处显示有关此方法的摘要
			%   此处显示详细说明
			outputArg = obj.Property1 + inputArg;
		end
	end
end