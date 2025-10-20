%[text] 断线重连失败后，还允许手动恢复会话
%[text] ## 示例
%[text] ```matlabCodeExample
%[text] %假设在BOX3服务器上运行的Formal3进程断线重连失败。手动解决连接问题后，需要先Server对象进行Initialize，然后可以恢复此进程
%[text] BOX3.Initialize('COM14',9600);
%[text] Formal3.RestoreSession;
%[text] ```
function RestoreSession(obj)
if obj.Server.AllProcesses.isConfigured
	obj.Server.AllProcesses(obj.Pointer)=[];
end
AsyncStream=obj.Server.AsyncStream;
obj.Pointer=typecast(AsyncStream.SyncInvoke(Gbec.UID.PortA_CreateProcess),obj.Server.PointerType);
obj.Server.AllProcesses(obj.Pointer)=matlab.lang.WeakReference(obj);
if obj.State==Gbec.UID.State_Idle
	return;
end
TrialsDone=obj.TrialRecorder.GetTimeTable();
if ~isempty(TrialsDone)
	TrialsDone(end,:)=[];
	obj.TrialIndex=obj.TrialIndex-1;
end
TrialsDone=groupsummary(TrialsDone,'Event');
NumDistinctTrials=height(TrialsDone);
obj.EventRecorder.LogEvent(Gbec.UID.Event_ConnectionReset);
LocalPort=AsyncStream.AllocatePort;
OCU=onCleanup(@()AsyncStream.ReleasePort(LocalPort));
TCO=Async_stream_IO.TemporaryCallbackOff(AsyncStream);
AsyncStream.BeginSend(Gbec.UID.PortA_RestoreModule,NumDistinctTrials*3+2+obj.Server.PointerSize);
AsyncStream<=LocalPort<=obj.Pointer<=obj.SessionID;
for T=1:NumDistinctTrials
	AsyncStream<=TrialsDone.Event(T)<=uint16(TrialsDone.GroupCount(T));
end
NumBytes=AsyncStream.Listen(LocalPort);
if NumBytes==1
	obj.ThrowResult(AsyncStream.Read);
else
	AsyncStream.Read(NumBytes);%清除多余的字节
	Gbec.Exception.Unexpected_response_from_Arduino.Throw;
end
end

%[appendix]{"version":"1.0"}
%---
