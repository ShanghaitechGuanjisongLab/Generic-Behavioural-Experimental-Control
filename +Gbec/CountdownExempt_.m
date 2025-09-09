classdef CountdownExempt_<handle
	%用于从Server的串口倒计时中豁免的令牌
	% 构造此对象将使拥有它的Server对象的串口倒计时停止，析构此对象将恢复倒计时。如果需要多次豁免，可以构造多个此类对象，倒计时将在最后一个对象析构后恢复。
	properties(SetAccess=immutable)
		%拥有此令牌的Server对象
		Server
	end
	methods
		function obj = CountdownExempt_(Server)
			%# 语法
			% ```
			% obj = Gbec.CountdownExempt_(Server);
			% ```
			%# 输入参数
			% Server(1,1)Gbec.Server，服务器对象
			obj.Server = Server;
			Server.CountdownExemptLeft = Server.CountdownExemptLeft + 1;
			Server.SerialCountdown.stop;
		end
		function delete(obj)
			obj.Server.CountdownExemptLeft = obj.Server.CountdownExemptLeft - 1;
			if obj.Server.CountdownExemptLeft == 0
				obj.Server.SerialCountdown.start;
			end
		end
	end
end