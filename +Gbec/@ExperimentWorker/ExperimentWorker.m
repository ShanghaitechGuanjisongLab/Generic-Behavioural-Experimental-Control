classdef ExperimentWorker<handle
	%实验主控制器
	%一般不直接使用该类，而是通过*_Client.mlx实时脚本作为用户界面来操纵实验。当然您也可以根据我们提供的Client脚本学习本类的使用方法。
	%此类不支持直接构造。使用静态方法New获取此类的实例。
	properties
		%实验记录保存路径。
		% 如果那个路径已经有数据库文件，将尝试合并，然后为多天的行为做学习曲线图。
		SavePath(1,1)string
		%实验结束后是否保存记录
		SaveFile(1,1)logical=true
		%喵提醒码
		MiaoCode(1,1)string=""
		%喵提醒重试次数
		HttpRetryTimes(1,1)uint8=3
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
		%主机动作，必须继承Gbec.IHostAction，用于执行Arduino无法执行的主机任务，例如在屏幕上显示图像。
		%See also Gbec.IHostAction
		HostAction
		%将要合并的旧数据路径
		MergeData=missing
	end
	properties(Access=protected)
		Serial Async_stream_IO.AsyncSerialStream
		State=Gbec.UID.State_SessionInvalid
		%没有对象无法初始化
		WatchDog timer
		SignalHandler
		TIC
		TimeOffset
	end
	properties(GetAccess=protected,SetAccess=immutable)
		EventRecorder MATLAB.DataTypes.EventLogger
		TrialRecorder MATLAB.DataTypes.EventLogger
		PreciseRecorder MATLAB.Containers.Vector
	end
	properties(Dependent)
		%设置多少秒无操作后自动关闭串口，默认3分钟
		ShutdownSerialAfter
	end
	methods(Access=protected)
		function AbortAndSave(obj)
			%此方法将负责启用看门狗
			obj.EventRecorder.LogEvent(Gbec.UID.State_SessionAborted);
			disp('会话已放弃');
			if ~isempty(obj.VideoInput)
				stop(obj.VideoInput);
			end
			if obj.SaveFile
				if questdlg('是否保存现有数据？','实验已放弃','确定','取消','确定')~="取消"
					obj.SaveInformation;
				else
					if isequaln(obj.MergeData,missing)
						delete(obj.SavePath);
					else
						[Directory,Filename]=fileparts(obj.SavePath);
						movefile(fullfile(Directory,Filename+".将合并.mat"),obj.SavePath,'f');
					end
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
						Gbec.Exceptions.Arduino_received_unsupported_API_code.Throw;
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
			if isempty(obj.TIC)
				obj.Serial.write(0,'uint32');
			else
				obj.Serial.write(milliseconds(seconds(toc(obj.TIC))+obj.TimeOffset),'uint32');
			end
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
				Gbec.Exceptions.Unexpected_response_from_Arduino.Throw;
			end
		end
		function SerialCallback(obj,~,~)
			while obj.Serial.NumBytesAvailable
				obj.SignalHandler(obj.Serial.read(1,'uint8'));
			end
		end
		function TestHandler(~,Signal)
			persistent SignalRecord
			if isempty(SignalRecord)
				SignalRecord=dictionary;
			end
			if SignalRecord.isConfigured&&SignalRecord.isKey(Signal)
				SR=SignalRecord(Signal)+1;
			else
				SR=1;
			end
			fprintf('%s：%u\n',Gbec.LogTranslate(Signal),SR);
			SignalRecord(Signal)=SR;
		end
		function HandleSignal(obj,Signal)
			if isempty(obj.SignalHandler)
				% Gbec.Exceptions.Unexpected_response_from_Arduino.Throw;
				disp(Gbec.UID(Signal));
			else
				obj.SignalHandler(Signal);
			end
		end
		RunningHandler(obj,Signal)
		function obj=ExperimentWorker
			%构造对象，建议使用MATLAB.Lang.Owner包装对象，不要直接存入工作区，否则清空变量时可能不能正确断开串口
			disp(['通用行为实验控制器' Gbec.Version().Me ' by 张天夫']);
			obj.WatchDog=timer(StartDelay=180,TimerFcn=@obj.ReleaseSerial);
			obj.EventRecorder=MATLAB.DataTypes.EventLogger;
			obj.TrialRecorder=MATLAB.DataTypes.EventLogger;
			obj.PreciseRecorder=MATLAB.Containers.Vector;
		end
		function FeedDog(obj)
			if obj.WatchDog.Running=="on"
				obj.WatchDog.stop;
				obj.WatchDog.start;
			end
		end
		function ReleaseSerial(obj,~,~)
			%此函数不能作为文件内私有函数，因为被文件外函数调用
			delete(obj.Serial);
			disp('串口已释放');
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
			obj.FeedDog;
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
						obj.WatchDog.start;
						break;
					case UID.State_SessionRunning
						disp('测试结束');
						break;
					case UID.Signal_NoLastTest
						Gbec.Exceptions.Last_test_not_running_or_unstoppable.Throw;
					case UID.Signal_NoSuchTest
						Gbec.Exceptions.Test_not_found_on_Arduino.Throw;
					otherwise
						obj.HandleSignal(Signal)
				end
			end
		end
		function PauseSession(obj)
			%暂停会话
			import Gbec.UID
			import Gbec.Exceptions
			obj.FeedDog;
			if obj.State==UID.State_SessionRestored||obj.State==UID.State_SessionPaused
				Exceptions.Cannot_pause_a_paused_session.Throw;
			end
			obj.ApiCall(UID.API_Pause);
			while true
				Signal=obj.WaitForSignal;
				switch Signal
					case UID.State_SessionInvalid
						Exceptions.No_sessions_are_running.Throw;
					case UID.State_SessionPaused
						obj.EventRecorder.LogEvent(UID.State_SessionPaused);
						disp('会话暂停');
						obj.Serial.configureCallback('off');
						obj.SignalHandler=function_handle.empty;
						obj.State=UID.State_SessionPaused;
						break;
					case UID.State_SessionAborted
						Exceptions.Cannot_pause_an_aborted_session.Throw;
					case UID.State_SessionFinished
						Exceptions.Cannot_pause_a_finished_session.Throw;
					otherwise
						obj.HandleSignal(Signal);
				end
			end
		end
		function AbortSession(obj)
			%放弃会话
			import Gbec.UID
			import Gbec.Exceptions
			obj.FeedDog;
			switch obj.State
				case UID.State_SessionRestored
					obj.AbortAndSave;
				case UID.State_SessionAborted
					Exceptions.Cannot_abort_an_aborted_session.Throw;
				otherwise
					try
						obj.ApiCall(UID.API_Abort);
					catch ME
						if ME.identifier=="MATLAB:class:InvalidHandle"
							obj.State=UID.State_SessionInvalid;
							Gbec.Exceptions.Serialport_disconnected.Throw('串口已断开，请重新初始化');
						end
					end
					while true
						Signal=obj.WaitForSignal;
						switch Signal
							case UID.State_SessionInvalid
								Exceptions.No_sessions_are_running.Throw;
							case UID.State_SessionAborted
								obj.Serial.configureCallback('off');
								obj.SignalHandler=function_handle.empty;
								obj.AbortAndSave;%这个函数调用包含了启用看门狗
								break;
							case UID.State_SessionFinished
								obj.State=UID.State_SessionFinished;
								Exceptions.Cannot_abort_a_finished_session.Throw;
								%异常信号应该直接忽略
						end
					end
			end
		end
		function delete(obj)
			delete(obj.WatchDog);
			delete(obj.Serial);
		end
		function SFT=get.ShutdownSerialAfter(obj)
			obj.FeedDog;
			SFT=obj.WatchDog.StartDelay;
		end
		function set.ShutdownSerialAfter(obj,SSA)
			WatchDogRunning=obj.WatchDog.Running=="on";
			obj.WatchDog.stop;
			obj.WatchDog.StartDelay=SSA;
			if WatchDogRunning
				obj.WatchDog.start;
			end
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
			obj.FeedDog;
			obj.ApiCall(UID.API_GetInfo);
			obj.Serial.write(SessionUID,'uint8');
			while true
				Signal=obj.WaitForSignal;
				switch Signal
					case UID.State_SessionInvalid
						Gbec.Exceptions.Must_run_session_before_getting_information.Throw;
					case UID.Signal_InfoStart
						Information=CollectStruct(obj.Serial);
						if ~isempty(obj.HostAction)
							Information.HostAction=obj.HostAction.GetInformation;
						end
						break;
					case UID.State_SessionRunning
						Gbec.Exceptions.Cannot_get_information_while_session_running.Throw;
					otherwise
						obj.HandleSignal(Signal);
				end
			end
		end
		function PeekState(EW)
			%观察会话当前的运行状态
			obj.FeedDog;
			EW.ApiCall(Gbec.UID.API_Peek);
			disp(Gbec.UID(EW.WaitForSignal));
		end
	end
end