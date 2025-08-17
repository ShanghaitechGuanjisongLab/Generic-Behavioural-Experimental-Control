classdef CountdownExempt_<handle
	%用于从Server的串口倒计时中豁免的令牌
	properties(SetAccess=immutable)
		Server
	end
	methods
		function obj = CountdownExempt_(Server)
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