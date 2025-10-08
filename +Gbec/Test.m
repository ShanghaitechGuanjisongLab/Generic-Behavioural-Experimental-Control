classdef Test<Gbec.Process
	%轻量级测试类进程，扩展一些便于手动测试设备的实用方法
	properties
		%可调用对象，收到Host类UID消息时调用
		HostAction
	end
	methods(Access=protected)
		function TestCycle(obj,Port,TestID,Times)
			arguments
				obj
				Port uint8
				TestID Gbec.UID
				Times uint16=0x001
			end
			obj.Server.FeedDogIfActive();
			AsyncStream=obj.Server.AsyncStream;
			TCO=Async_stream_IO.TemporaryCallbackOff(AsyncStream);
			AsyncStream.Send(Async_stream_IO.ArgumentSerialize(Port,obj.Pointer,TestID,Times),Gbec.UID.PortA_StartModule);
			NumBytes=AsyncStream.Listen(Port);
			if NumBytes
				Exception=AsyncStream.Read;
				AsyncStream.Read(NumBytes-1);
				obj.ThrowResult(Exception);
			else
				Gbec.Exception.StartModule_no_return.Throw;
			end
		end
		function [Port,OCU]=GetPortOcu(obj)
			AsyncStream=obj.Server.AsyncStream;
			if~isvalid(AsyncStream)
				obj.delete;
				Gbec.Exception.Server_abandoned.Throw('请重新连接Server');
			end
			Port=AsyncStream.AllocatePort;
			OCU=onCleanup(@()AsyncStream.ReleasePort(Port));
		end
	end
	methods
		function obj = Test(Server)
			%# 语法
			% ```
			% obj = Gbec.Test(Server);
			% ```
			%# 输入参数
			% Server(1,1)Gbec.Server，目标Server
			obj@Gbec.Process(Server);
		end
		function RepeatCheck(obj,TestID,NumTimes)
			%重复执行某项测试
			%# 语法
			% ```
			% obj.RepeatCheck(TestID);
			% %执行指定ID的测试，将弹出对话框询问执行次数
			%
			% obj.RepeatCheck(TestID,NumTimes);
			% %执行指定ID的测试NumTimes次
			% ```
			%# 输入参数
			% TestID(1,1)Gbec.UID，待检查的测试ID
			% NumTimes(1,1)uint16，检查次数
			arguments
				obj
				TestID(1,1)Gbec.UID
				NumTimes(1,1)uint16=str2double(inputdlg('检查几次？','检查几次？'))
			end
			fprintf('%s×%u……\n',TestID,NumTimes);
			[Port,OCU]=GetPortOcu;
			obj.TestCycle(Port,TestID,NumTimes);
		end
		function OneEnterOneCheck(obj,TestID,Prompt)
			%每次按回车执行一次测试
			%# 语法
			% ```
			% obj.OneEnterOneCheck(TestID,Prompt);
			% ```
			%# 输入参数
			% TestID(1,1)Gbec.UID，待检查的测试ID
			% Prompt(1,1)string，提示字符串
			arguments
				obj
				TestID(1,1)Gbec.UID
				Prompt
			end
			[Port,OCU]=GetPortOcu;
			while input(Prompt,"s")==""
				obj.TestCycle(Port,TestID);
			end
		end
		function StartCheck(obj,TestID)
			%开始执行某项测试
			%# 语法
			% ```
			% obj.StartCheck(TestID);
			% ```
			%# 输入参数
			% TestID(1,1)Gbec.UID，待检查的测试ID
			arguments
				obj
				TestID(1,1)Gbec.UID
			end
			fprintf('%s……\n',TestID);
			[Port,OCU]=GetPortOcu;
			obj.TestCycle(Port,TestID);
		end
		function StopCheck(obj)
			%停止当前测试
			obj.Server.FeedDogIfActive();
			obj.ThrowResult(obj.Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_AbortProcess,obj.Pointer));
			disp('测试结束');
		end
		function Signal_(obj,Message)
			%此方法由Server调用，用户不应使用
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
		function ConnectionReset_(obj)
			%此方法由Server调用，派生类负责处理，用户不应使用
			obj.Server.AllProcesses(obj.Pointer)=[];
			AsyncStream=obj.Server.AsyncStream;
			obj.Pointer=typecast(AsyncStream.SyncInvoke(Gbec.UID.PortA_CreateProcess),obj.Server.PointerType);
			obj.Server.AllProcesses(obj.Pointer)=matlab.lang.WeakReference(obj);
		end
	end
end