classdef Test<Gbec.Process
	%轻量级测试类进程，扩展一些便于手动测试设备的实用方法

	properties
		%可调用对象，收到Host类UID消息时调用
		HostAction
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
			obj.Server.FeedDogIfActive();
			fprintf('%s×%u……\n',TestID,NumTimes);
			obj.ThrowResult(obj.Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_StartModule,obj.Pointer,TestID,NumTimes));
		end
		function OneEnterOneCheck(obj,TestID,Prompt)
			arguments
				obj
				TestID(1,1)Gbec.UID
				Prompt
			end
			obj.Server.FeedDogIfActive();
			while input(Prompt,"s")==""
				obj.Server.FeedDogIfActive();
				obj.ThrowResult(obj.Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_StartModule,obj.Pointer,TestID,0x001));
			end
		end
		function StartCheck(obj,TestID)
			arguments
				obj
				TestID(1,1)Gbec.UID
			end
			obj.Server.FeedDogIfActive();
			fprintf('%s……\n',TestID);
			obj.ThrowResult(obj.Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_StartModule,obj.Pointer,TestID,0x001));
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