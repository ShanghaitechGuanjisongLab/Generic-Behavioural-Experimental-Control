classdef Process<handle
	properties(SetAccess=immutable)
		%该Process从属的Server
		Server
	end
	properties(SetAccess=protected)
		%该Process的指针，在故障恢复时可能修改
		Pointer
	end
	methods(Access=protected,Static)
		function WarnResult(Result)
			Result=Gbec.UID(Result);
			if Result~=Gbec.UID.Exception_Success
				Result.Warn;
			end
		end
	end
	methods(Access=protected)
		function ThrowResult(obj,Result)
			Result=Gbec.UID(Result(1));
			if Result==Gbec.UID.Exception_InvalidProcess
				obj.Server.RefreshAllProcesses;
				obj.delete;
			end
			if Result~=Gbec.UID.Exception_Success
				Result.Throw;
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
			Server.FeedDogIfActive();
			obj.Server=Server;
			if nargin>1
				obj.Pointer=cast(Pointer,Server.PointerType);
			else
				obj.Pointer=typecast(Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_CreateProcess),Server.PointerType);
			end
			Server.AllProcesses(obj.Pointer)=matlab.lang.WeakReference(obj);
		end
		function ProcessFinished_(~)
			%此方法由Server调用，派生类负责处理，用户不应使用
		end
		function Signal_(~,~)
			%此方法由Server调用，派生类负责处理，用户不应使用
		end
		function TrialStart_(~,~)
			%此方法由Server调用，派生类负责处理，用户不应使用
		end
		function ConnectionReset_(~)
			%此方法由Server调用，派生类负责处理，用户不应使用
		end
		function delete(obj)
			if obj.Server.isvalid
				obj.Server.FeedDogIfActive();
				try
					obj.Server.AllProcesses.remove(obj.Pointer);
				catch ME
					if ME.identifier~="MATLAB:dictionary:UnconfiguredRemovalNotSupported"
						ME.rethrow;
					end
				end
				try
					obj.ThrowResult(obj.Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_DeleteProcess,obj.Pointer));
				catch ME
					if all(ME.identifier~=["Async_stream_IO:Exception:Serial_not_respond_in_time","Gbec:UID:Exception_InvalidProcess","MATLAB:class:InvalidHandle"])
						ME.rethrow;
					end
				end
			end
		end
		function V=IsValid(obj)
			%检查当前进程是否有效
			V=obj.isvalid;
			if~V
				return;
			end
			V=obj.Server.isvalid;
			if~V
				return;
			end
			V=obj.Server.AllProcesses.numEntries;
			if~V
				return;
			end
			V=obj.Server.AllProcesses.isKey(obj.Pointer);
		end
	end
end