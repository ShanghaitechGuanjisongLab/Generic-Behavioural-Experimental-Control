classdef ExperimentWorker<handle
	%实验主控制器
	%一般不直接使用该类，而是通过*_Client.mlx实时脚本作为用户界面来操纵实验。当然您也可以根据我们提供的Client脚本学习本类的使用方法。
	%此类不支持直接构造。使用静态方法New获取此类的实例。
	properties
		%实验记录保存路径
		SavePath(1,1)string
		%实验结束后是否保存记录
		SaveFile(1,1)logical
		%喵提醒码
		MiaoCode(1,1)string=""
		%喵提醒重试次数
		HttpRetryTimes(1,1)uint8=3
		%会话结束后是否自动关闭串口
		ShutDownSerialAfterSession(1,1)logical
		%当前运行会话
		SessionUID(1,1)Gbec.UID
		%日期时间
		DateTime
		%鼠名
		Mouse string
		%断线重连尝试间隔秒数
		RetryInterval(1,1)double=3
		%断线重连尝试次数
		MaxRetryTimes(1,1)uint8=3
		%视频对象
		VideoInput
		%当前设计的回合总数
		DesignedNumTrials
		%提醒检查回合周期
		CheckCycle=30
		%主机动作，用于执行Arduino无法执行的主机任务，例如在屏幕上显示图像。
		%See also Gbec.IHostAction
		HostAction Gbec.IHostAction
	end
	properties(Access=protected)
		Serial internal.Serialport
		State=Gbec.UID.State_SessionInvalid
		%没有对象无法初始化
		WatchDog
		SignalHandler
	end
	properties(GetAccess=protected,SetAccess=immutable)
		EventRecorder MATLAB.DataTypes.EventLogger
		TrialRecorder MATLAB.DataTypes.EventLogger
	end
	properties(Dependent)
		%如果启用会话结束后自动关闭串口功能，该属性设置关闭串口的延迟时间
		SerialFreeTime(1,1)double
	end
	methods(Access=protected)
		function AbortAndSave(obj)
			obj.EventRecorder.LogEvent(Gbec.UID.State_SessionAborted);
			disp('会话已放弃');
			if ~isempty(obj.VideoInput)
				stop(obj.VideoInput);
			end
			if obj.SaveFile
				if questdlg('是否保存现有数据？','实验已放弃','确定','取消','确定')~="取消"
					obj.SaveInformation;
				else
					delete(obj.SavePath);
				end
			else
				warning("数据未保存");
			end
			obj.State=Gbec.UID.State_SessionAborted;
			obj.WatchDog.start;
		end
		function Signal=WaitForSignal(obj)
			obj.Serial.Timeout=1;
			while true
				Signal=obj.Serial.read(1,'uint8');
				if isempty(Signal)
					warning("Arduino设备在%us内未响应，请检查连接，或继续等待程序重试",obj.Serial.Timeout);
					obj.Serial.Timeout=obj.Serial.Timeout*2;
				else
					break;
				end
			end
		end
		function ApiCall(obj,ApiUid)
			obj.Serial.write(ApiUid,'uint8');
			while true
				%这里可能会有不断发送的中断信号干扰，这些信号无用但不构成错误，需要过滤掉
				Signal=obj.WaitForSignal;
				switch Signal
					case Gbec.UID.Signal_ApiFound
						break
					case Gbec.UID.Signal_ApiInvalid
						Gbec.GbecException.Arduino_received_unsupported_API_code.Throw;
					otherwise
						obj.HandleSignal(Signal);
				end
			end
		end
		function RestoreSession(obj)
			TrialsDone=obj.TrialRecorder.GetTimeTable().Event;
			if ~isempty(TrialsDone)
				TrialsDone(end)=[];
			end
			DistinctTrials=unique(TrialsDone);
			NumTrials=countcats(categorical(TrialsDone));
			obj.ApiCall(Gbec.UID.API_Restore);
			obj.Serial.write(obj.SessionUID,"uint8");
			NDT=numel(DistinctTrials);
			obj.Serial.write(NDT,'uint8');
			for T=1:NDT
				obj.Serial.write(DistinctTrials(T),'uint8');
				obj.Serial.write(NumTrials(T),'uint16');
			end
			if obj.WaitForSignal==Gbec.UID.Signal_SessionRestored
				obj.SignalHandler=@obj.RunningHandler;
				obj.Serial.configureCallback("byte",1,@obj.SerialCallback);
			else
				Gbec.GbecException.Unexpected_response_from_Arduino.Throw;
			end
		end
		function SerialCallback(obj,~,~)
			while obj.Serial.NumBytesAvailable
				obj.SignalHandler(obj.Serial.read(1,'uint8'));
			end
		end
		function TestHandler(~,Signal)
			persistent SignalIndex
			if isempty(SignalIndex)
				SignalIndex=0;
			end
			if Signal==Gbec.UID.Signal_MonitorHit
				SignalIndex=SignalIndex+1;
				disp("成功检测到触摸信号："+num2str(SignalIndex));
			else
				warning('收到非法信号');
			end
		end
		function HandleSignal(obj,Signal)
			if isempty(obj.SignalHandler)
				Gbec.GbecException.Unexpected_response_from_Arduino.Throw;
			else
				obj.SignalHandler(Signal);
			end
		end
		RunningHandler(obj,Signal)
		function obj=ExperimentWorker
			%构造对象，建议使用MATLAB.Lang.Owner包装对象，不要直接存入工作区，否则清空变量时可能不能正确断开串口
			disp(['通用行为实验控制器' Gbec.Version().Me ' by 张天夫']);
			obj.WatchDog=timer(StartDelay=10,TimerFcn=@(~,~)ReleaseSerial(obj.Serial));
			obj.EventRecorder=MATLAB.DataTypes.EventLogger;
			obj.TrialRecorder=MATLAB.DataTypes.EventLogger;
		end
	end
	methods(Static)
		function obj=New
			%获取一个用MATLAB.Lang.Owner包装的ExperimentWorker实例。
			%Owner会将对对象的所有操作转发给其所包装的实例。始终通过Owner访问ExperimentWorker，不要使用裸的
			% ExperimentWorker句柄，否则可能造成资源无法正确释放。
			%# 语法
			% ```
			% obj=Gbec.ExperimentWorker.New;
			% ```
			%# 返回值
			% obj(1,1)MATLAB.Lang.Owner<ExperimentWorker>
			%See also MATLAB.Lang.Owner
			obj=MATLAB.Lang.Owner(Gbec.ExperimentWorker);
		end
	end
	methods
		function SerialInitialize(obj,SerialPort)
			%初始化串口
			%# 语法
			% ```MATLAB
			% obj.SerialInitialize(SerialPort);
			% ```
			%# 输入参数
			% SerialPort(1,1)string，串口名称
			try
				assert(obj.Serial.Port==SerialPort);
				obj.ApiCall(Gbec.UID.API_IsReady);
				assert(obj.WaitForSignal==Gbec.UID.Signal_SerialReady);
			catch
				delete(obj.Serial);
				obj.Serial=serialport(SerialPort,9600);
				%刚刚初始化时不能向串口发送数据，只能等待Arduino主动宣布初始化完毕
				if obj.WaitForSignal==Gbec.UID.Signal_SerialReady
					obj.Serial.write(randi(intmax('uint32'),'uint32'),'uint32');
				else
					Gbec.GbecException.Serial_handshake_failed.Throw;
				end
				obj.WatchDog.TimerFcn=@(~,~)ReleaseSerial(obj.Serial);
				obj.Serial.ErrorOccurredFcn=@obj.InterruptRetry;
			end
			obj.WatchDog.stop;
			obj.Serial.configureCallback("off");
			obj.SignalHandler=function_handle.empty;
		end
		function StopTest(obj,TestUID)
			%停止测试
			%仅适用于手动结束类测试。自动结束类测试不能停止。
			%# 语法
			% ```MATLAB
			% obj.StopTest(TestUID);
			% ```
			%# 输入参数
			% TestUID(1,1)Gbec.UID=Gbec.UID.Test_Last，要停止的测试UID，只能选择手动结束类测试进行停止。默认停止上一个开始的测试。
			arguments
				obj
				TestUID=Gbec.UID.Test_Last
			end
			import Gbec.UID
			obj.ApiCall(UID.API_TestStop);
			obj.Serial.write(TestUID,'uint8');
			while true
				%这里可能会有不断发送的中断信号干扰，这些信号无用但不构成错误，需要过滤掉
				Signal=obj.WaitForSignal;
				switch Signal
					case UID.Signal_TestStopped
						disp('测试结束');
						obj.Serial.configureCallback('off');
						obj.SignalHandler=function_handle.empty;
						break;
					case UID.State_SessionRunning
						disp('测试结束');
						break;
					case UID.Signal_NoLastTest
						Gbec.GbecException.Last_test_not_running_or_unstoppable.Throw;
					case UID.Signal_NoSuchTest
						Gbec.GbecException.Test_not_found_on_Arduino.Throw;
					otherwise
						obj.HandleSignal(Signal);
				end
			end
		end
		function PauseSession(obj)
			%暂停会话
			import Gbec.UID
			import Gbec.GbecException
			if obj.State==UID.State_SessionRestored||obj.State==UID.State_SessionPaused
				GbecException.Cannot_pause_a_paused_session.Throw;
			end
			obj.ApiCall(UID.API_Pause);
			while true
				Signal=obj.WaitForSignal;
				switch Signal
					case UID.State_SessionInvalid
						GbecException.No_sessions_are_running.Throw;
					case UID.State_SessionPaused
						obj.EventRecorder.LogEvent(UID.State_SessionPaused);
						disp('会话暂停');
						obj.Serial.configureCallback('off');
						obj.SignalHandler=function_handle.empty;
						obj.State=UID.State_SessionPaused;
						break;
					case UID.State_SessionAborted
						GbecException.Cannot_pause_an_aborted_session.Throw;
					case UID.State_SessionFinished
						GbecException.Cannot_pause_a_finished_session.Throw;
					otherwise
						obj.HandleSignal(Signal);
				end
			end
		end
		function AbortSession(obj)
			%放弃会话
			import Gbec.UID
			import Gbec.GbecException
			switch obj.State
				case UID.State_SessionRestored
					obj.AbortAndSave;
				case UID.State_SessionAborted
					GbecException.Cannot_abort_an_aborted_session.Throw;
				otherwise
					obj.ApiCall(UID.API_Abort);
					while true
						Signal=obj.WaitForSignal;
						switch Signal
							case UID.State_SessionInvalid
								GbecException.No_sessions_are_running.Throw;
							case UID.State_SessionAborted
								obj.Serial.configureCallback('off');
								obj.SignalHandler=function_handle.empty;
								obj.AbortAndSave;%这个函数调用包含了启用看门狗
								break;
							case UID.State_SessionFinished
								GbecException.Cannot_abort_a_finished_session.Throw;
							otherwise
								obj.HandleSignal(Signal);
						end
					end
			end
		end
		function delete(obj)
			delete(obj.WatchDog);
			delete(obj.Serial);
		end
		function SFT=get.SerialFreeTime(obj)
			SFT=obj.WatchDog.StartDelay;
		end
		function set.SerialFreeTime(obj,SFT)
			obj.WatchDog.StartDelay=SFT;
		end
		function Information = GetInformation(obj,SessionUID)
			%获取会话信息
			%# 语法
			% ```MATLAB
			% Info=obj.GetInformation;
			% %获取上次运行的会话信息
			%
			% Info=obj.GetInformation(SessionUID);
			% %获取指定UID的会话信息
			% ```
			%# 输入参数
			% SessionUID(1,1)Gbec.UID=Gbec.UID.Session_Current，要获取的会话UID，默认获取之前运行的会话信息
			%# 返回值
			% Information(1,1)struct，信息结构体
			arguments
				obj Gbec.ExperimentWorker
				SessionUID=Gbec.UID.Session_Current
			end
			import Gbec.UID
			obj.WatchDog.stop;
			obj.ApiCall(UID.API_GetInfo);
			obj.Serial.write(SessionUID,'uint8');
			while true
				Signal=obj.WaitForSignal;
				switch Signal
					case UID.State_SessionInvalid
						Gbec.GbecException.Must_run_session_before_getting_information.Throw;
					case UID.Signal_InfoStart
						Information=CollectStruct(obj.Serial);
						if ~isempty(obj.HostAction)
							Information.HostAction=obj.HostAction.GetInformation;
						end
						break;
					case UID.State_SessionRunning
						Gbec.GbecException.Cannot_get_information_while_session_running.Throw;
					otherwise
						obj.HandleSignal(Signal);
				end
			end
		end
		function PeekState(EW)
			%观察会话当前的运行状态
			EW.ApiCall(Gbec.UID.API_Peek);
			disp(Gbec.UID(EW.WaitForSignal));
		end
	end
end
function ReleaseSerial(Serial)
delete(Serial);
disp('串口已释放');
end