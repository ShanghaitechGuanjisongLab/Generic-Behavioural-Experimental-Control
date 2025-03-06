function InterruptRetry(obj,~,~)
import Gbec.UID
import Gbec.Exception
RunningOrPaused=obj.State==UID.State_SessionRunning||obj.State==UID.State_SessionPaused;
if RunningOrPaused
	obj.EventRecorder.LogEvent(UID.Event_SerialInterrupt);
end
Suffix="/"+string(obj.MaxRetryTimes)+"次";
SerialPort=obj.Serial.Port;
obj.LogPrint("串口连接中断");
ReconnectFail=true;
for a=1:obj.MaxRetryTimes
	disp("，正尝试恢复连接第"+string(a)+Suffix);
	pause(obj.RetryInterval);
	try
		obj.SerialInitialize(SerialPort,false);
		ReconnectFail=false;
		break;
	catch ME
		obj.LogPrint(sprintf("串口同步失败：%s：%s",ME.identifier,ME.message));
	end
end
if ReconnectFail
	obj.State=UID.State_SessionInvalid;
	if obj.MiaoCode~=""
		SendMiao("Arduino连接中断，请检查设备！",obj.HttpRetryTimes,obj.MiaoCode);
	end
	Exception.Disconnection_reconnection_failed.Throw;
end
obj.LogPrint("重新连接成功");
if RunningOrPaused
	obj.EventRecorder.LogEvent(UID.Event_SerialReconnect);
	if obj.State==UID.State_SessionRunning
		obj.RestoreSession;
	else
		obj.State=UID.State_SessionRestored;
	end
end
end