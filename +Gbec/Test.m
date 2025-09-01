classdef Test<Gbec.Process
	%轻量级测试类进程，扩展一些便于手动测试设备的实用方法

	properties
		%可调用对象，收到Host类UID消息时调用
		HostAction
	end
	methods(Access=protected)
		function TestCycle(obj,Port,TestID,Times)
			obj.Server.FeedDogIfActive();
			AsyncStream=obj.Server.AsyncStream;
			TCO=Async_stream_IO.TemporaryCallbackOff(AsyncStream.Serial);
			AsyncStream.Send(Async_stream_IO.ArgumentSerialize(Port,obj.Pointer,TestID,Times),Gbec.UID.PortA_StartModule);
			NumBytes=AsyncStream.Listen(Port);
			if NumBytes
				obj.ThrowResult(AsyncStream.Read);
				AsyncStream.Read(NumBytes-1);
			else
				Gbec.Exception.StartModule_no_return.Throw;
			end
		end
	end
	methods
		function obj = Test(Server)
			obj@Gbec.Process(Server);
		end
		function RepeatCheck(obj,TestID,NumTimes)
			arguments
				obj
				TestID(1,1)Gbec.UID
				NumTimes(1,1)uint16=str2double(inputdlg('检查几次？','检查几次？'))
			end
			fprintf('%s×%u……\n',TestID,NumTimes);
			obj.Server.FeedDogIfActive();
			AsyncStream=obj.Server.AsyncStream;
			Port=AsyncStream.AllocatePort();
			TCO=Async_stream_IO.TemporaryCallbackOff(AsyncStream.Serial);
			AsyncStream.Send(Async_stream_IO.ArgumentSerialize(Port,obj.Pointer,TestID,NumTimes),Gbec.UID.PortA_StartModule);
			NumBytes=AsyncStream.Listen(Port);
			if NumBytes
				obj.ThrowResult(AsyncStream.Read);
				AsyncStream.Read(NumBytes-1);
			else
				Gbec.Exception.StartModule_no_return.Throw;
			end
		end
		function OneEnterOneCheck(obj,TestID,Prompt)
			arguments
				obj
				TestID(1,1)Gbec.UID
				Prompt
			end
			obj.Server.FeedDogIfActive();
			AsyncStream=obj.Server.AsyncStream;
			Port=AsyncStream.AllocatePort();
			while input(Prompt,"s")==""
				obj.Server.FeedDogIfActive();
				TCO=Async_stream_IO.TemporaryCallbackOff(AsyncStream.Serial);
				AsyncStream.Send(Async_stream_IO.ArgumentSerialize(Port,obj.Pointer,TestID),Gbec.UID.PortA_StartModule);
				NumBytes=AsyncStream.Listen(Port);
				if NumBytes
					obj.ThrowResult(AsyncStream.Read);
					AsyncStream.Read(NumBytes-1);
				else
					Gbec.Exception.StartModule_no_return.Throw;
				end
				delete(TCO);
			end
		end
		function StartCheck(obj,TestID)
			arguments
				obj
				TestID(1,1)Gbec.UID
			end
			obj.Server.FeedDogIfActive();
			fprintf('%s……\n',TestID);
			AsyncStream=obj.Server.AsyncStream;
			Port=AsyncStream.AllocatePort();
			TCO=Async_stream_IO.TemporaryCallbackOff(AsyncStream.Serial);
			AsyncStream.Send(Async_stream_IO.ArgumentSerialize(Port,obj.Pointer,TestID),Gbec.UID.PortA_StartModule);
			NumBytes=AsyncStream.Listen(Port);
			if NumBytes
				obj.ThrowResult(AsyncStream.Read);
				AsyncStream.Read(NumBytes-1);
			else
				Gbec.Exception.StartModule_no_return.Throw;
			end
		end
		function StopCheck(obj)
			obj.Server.FeedDogIfActive();
			obj.ThrowResult(obj.Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_AbortProcess,obj.Pointer));
			disp('测试结束');
		end
		function Signal_(obj,Message)
			persistent Count
			obj.Server.FeedDogIfActive;
			if isempty(Count)
				Count = 0;
			end
			Message=Gbec.UID(Message);
			if startsWith(string(Message),'Host_')
				obj.HostAction(Message);
			else
				Count = Count + 1;
				fprintf('%s %s：%u\n',datetime,Gbec.LogTranslate(Message),Count);
			end
		end
	end
end