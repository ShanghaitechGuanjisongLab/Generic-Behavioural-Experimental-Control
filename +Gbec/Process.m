classdef Process<handle
	properties(SetAccess=?Gbec.Server)
		Server
		Handle
	end
	methods
		function obj = Process(Server,Handle)
			obj.Server=Server;
			obj.Handle=Handle;
		end
	end
end