classdef Process<handle
	properties(SetAccess=immutable)
		%该Process从属的Server
		Server
		%该Process的指针
		Pointer
	end
	methods(Access=protected,Static)
		function ThrowResult(Result)
			Result=Gbec.UID(Result);
			if Result~=Gbec.UID.Exception_Success
				Result.Throw;
			end
		end
		function WarnResult(Result)
			Result=Gbec.UID(Result);
			if Result~=Gbec.UID.Exception_Success
				Result.Warn;
			end
		end
	end
	methods
		function obj = Process(Server,Pointer)
			%引用指定Server上现有Process，或创建新Process
			%# 语法
			% ```
			% obj = Process(Server);
			% %在Server上创建新Process
			%
			% obj = Process(Server,Pointer);
			% %引用指定Server上现有Process
			% ```
			%# 输入参数
			% Server(1,1)Gbec.Server，目标Server
			% Pointer(1,1)，进程指针，类型取决于Server.PointerSize
			obj.Server=Server;
			if nargin>1
				obj.Pointer=cast(Pointer,Server.PointerType);
				%这个分支不喂狗，因为通常是Server.RefreshAllProcess在循环调用
			else
				obj.Server.FeedDogIfActive();
				obj.Pointer=Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_CreateProcess);
			end
			Server.AllProcesses(obj.Pointer)={obj};
		end
		function ProcessFinished(~)
			%此方法由Server调用，派生类负责处理
		end
		function Signal(~,~)
			%此方法由Server调用，派生类负责处理
		end
		function TrialStart(~,~)
			%此方法由Server调用，派生类负责处理
		end
		function delete(obj)
			obj.Server.FeedDogIfActive();
			Gbec.Process.WarnResult(obj.Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_DeleteProcess,obj.Pointer));
			obj.Server.AllProcesses.remove(obj.Pointer);
		end
	end
end