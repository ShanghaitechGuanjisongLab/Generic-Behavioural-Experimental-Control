classdef Formal<Gbec.Process
	%正规实验进程，提供完善可靠的数据收集系统和自定义接口
	properties(Constant,GetAccess=protected)
		DurationRep='uint32'
	end
	properties
		%提醒检查回合周期
		CheckCycle=30

		%日期时间
		DateTime

		%主机动作，必须继承Gbec.IHostAction，用于执行Arduino无法执行的主机任务，例如在屏幕上显示图像。
		%See also Gbec.IHostAction
		HostActions(1,1)dictionary

		%在输出日志中要前缀的名称
		LogName

		%喵提醒码
		MiaoCode(1,1)string=""

		%鼠名
		Mouse string

		%实验结束后是否保存记录
		SaveFile(1,1)logical=true

		%当前运行会话
		SessionID(1,1)Gbec.UID

		%会话后展示事件记录的参数
		%会话后，将调用UniExp.TrialwiseEventPlot，将本会话的事件记录表作为第一个参数，此属性值元胞展开作为后续参数。此用法必须安装统一实验分析作图。
		%此外，也可以调用任何自定义的函数句柄，将自定义的函数句柄放在第一个元胞中即可。此用法不依赖其它工具箱。此句柄必须接受包含以下列的表作为第一个输入：
		% - Time(:,1)duration，从会话开始经历的时间
		% - Event(:,1)categorical，事件
		% TepArguments的后续元胞将作为随后的参数展开输入给提供的函数句柄。
		%See also UniExp.TrialwiseEventPlot
		TepArguments

		%实验结束前几个回合发送喵提醒？
		TrialsBeforeEndRemind=1

		%视频对象
		VideoInput

		%喵提醒HTTP请求重试次数
		HttpRetryTimes=1
	end
	properties(SetAccess=protected)
		%指示实验当前是否正在运行中
		State=Gbec.UID.State_Idle

		%当前设计的回合总数。65535表示无法确定回合总数，可能是随机的。
		DesignedNumTrials

		%当前回合序号
		TrialIndex
	end
	properties(Access=protected)
		MergeData
		oSavePath
		OverwriteExisting=true
		ConnectionResetListener
		CountdownExempt
	end
	properties(Dependent)
		%数据保存路径
		% 如果那个路径已经有数据库文件，将尝试合并。文件存在性和可写性将会在设置该属性时立即检查，如果失败则此属性值不变。
		SavePath
	end
	properties(SetAccess=immutable)
		%事件记录器，可以查看当前已记录的事件
		EventRecorder MATLAB.DataTypes.EventLogger

		%回合记录器，可以查看当前已进行的回合
		TrialRecorder MATLAB.DataTypes.EventLogger
	end
	methods(Access=protected)
		function LogPrint(obj,Formatter,varargin)
			fprintf("\n%s："+Formatter,obj.LogName,varargin{:});
		end
	end
	methods
		function obj = Formal(Server)
			%# 语法
			% ```
			% obj = Gbec.Formal(Server);
			% ```
			%# 输入参数
			% Server(1,1)Gbec.Server，服务器对象
			obj@Gbec.Process(Server);
			obj.LogName=obj.Server.Name;
			obj.EventRecorder=MATLAB.DataTypes.EventLogger;
			obj.TrialRecorder=MATLAB.DataTypes.EventLogger;
		end
		function SP=get.SavePath(obj)
			SP=obj.oSavePath;
		end
		function set.SavePath(obj, SP)
			FileExists=isfile(SP);
			%判断是否应该覆盖保存数据。文件不存在，或虽然存在但用户确认覆盖的情况应该覆盖；文件存在但可以执行UniExp合并时不覆盖；文件存在但无法合并，用户也拒绝覆盖则报错，拒绝此次SavePath修改。
			obj.Server.FeedDogIfActive;
			if FileExists
				if isempty(which('UniExp.Version'))
					if questdlg('未找到统一实验分析作图工具箱，无法合并已存在的文件','是否覆盖？','是','否','否')=="是"
						obj.OverwriteExisting=true;
					else
						Gbec.Exception.Fail_to_merge_existing_dataset.Throw;
					end
				else
					obj.LogPrint("目标文件已存在，将尝试合并");
					try
						UniExp.DataSet(SP);
						obj.OverwriteExisting=false;
					catch ME
						if questdlg('合并失败，是否要覆盖文件？',ME.identifier,'是','否','否')=="是"
							obj.OverwriteExisting=true;
						else
							Gbec.Exception.Fail_to_merge_existing_dataset.Throw;
						end
					end
				end
			else
				obj.OverwriteExisting=true;
			end
			Fid=fopen(SP,'a');
			if Fid==-1
				mkdir(fileparts(SP));
				Fid=fopen(SP,'a');
			end
			if Fid==-1
				Gbec.Exception.No_right_to_write_SavePath.Throw;
			end
			fclose(Fid);
			if ~FileExists
				delete(SP);%如不删除，下次修改SavePath时会出错
			end
			obj.oSavePath=SP;
		end
		function TrialStart_(obj,TrialID)
			%此方法由Server调用，派生类负责处理，用户不应使用
			TrialID=Gbec.UID(TrialID);
			%这里必须记录UID而不是字符串，因为还要用于断线重连
			obj.TrialRecorder.LogEvent(TrialID);
			obj.EventRecorder.LogEvent(Gbec.UID.Event_TrialStart)
			obj.TrialIndex=obj.TrialIndex+1;
			TrialMod=mod(obj.TrialIndex,obj.CheckCycle);
			if obj.MiaoCode~=""
				if obj.TrialIndex==obj.DesignedNumTrials
					SendMiao('实验结束',obj.HttpRetryTimes,obj.MiaoCode);
				elseif TrialMod==0
					SendMiao(sprintf('已到%u回合，请检查实验状态',obj.TrialIndex),obj.HttpRetryTimes,obj.MiaoCode);
				end
			end
			if TrialMod==1&&obj.TrialIndex>1
				cprintf([1,0,1],'\n已过%u回合，请检查实验状态',obj.TrialIndex-1);
			end
			FprintfInCommandWindow('\n回合%u-%s：',obj.TrialIndex,TrialID);
		end
		function Signal_(obj,S)
			%此方法由Server调用，派生类负责处理，用户不应使用
			S=Gbec.UID(S);
			obj.EventRecorder.LogEvent(S);
			if obj.HostActions.numEntries&&obj.HostActions.isKey(S)
				obj.HostActions{S}.Run();
			else
				FprintfInCommandWindow(' %s',Gbec.LogTranslate(S));
			end
		end
		function ProcessFinished_(obj)
			%此方法由Server调用，派生类负责处理，用户不应使用
			obj.LogPrint('会话完成');
			if obj.SaveFile
				obj.SaveInformation;
			else
				FprintfInCommandWindow(" 数据未保存");
			end
			obj.State=Gbec.UID.State_Idle;
			obj.CountdownExempt.delete;
		end
		function ConnectionReset_(obj)
			%此方法由Server调用，派生类负责处理，用户不应使用
			obj.Server.AllProcesses(obj.Pointer)=[];
			AsyncStream=obj.Server.AsyncStream;
			obj.Pointer=typecast(AsyncStream.SyncInvoke(Gbec.UID.PortA_CreateProcess),obj.Server.PointerType);
			obj.Server.AllProcesses(obj.Pointer)=matlab.lang.WeakReference(obj);
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
		function PauseSession(obj)
			%暂停会话
			if obj.State~=Gbec.UID.State_Running
				Gbec.Exception.Process_not_running.Throw;
			end
			obj.ThrowResult(obj.Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_PauseProcess,obj.Pointer));
			obj.State=Gbec.UID.State_Paused;
			obj.EventRecorder.LogEvent(Gbec.UID.Event_ProcessPaused);
			obj.LogPrint('会话暂停');
			obj.CountdownExempt.delete;
		end
		function ContinueSession(obj)
			%继续会话
			if obj.State~=Gbec.UID.State_Paused
				Gbec.Exception.Process_not_paused.Throw;
			end
			obj.ThrowResult(obj.Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_ContinueProcess,obj.Pointer));
			obj.State=Gbec.UID.State_Running;
			obj.EventRecorder.LogEvent(Gbec.UID.Event_ProcessContinue);
			obj.LogPrint('会话继续');
			obj.CountdownExempt=Gbec.CountdownExempt_(obj.Server);
		end
		function AbortSession(obj)
			%放弃会话
			if obj.State==Gbec.UID.State_Idle
				Gbec.Exception.Process_not_running.Throw;
			end
			obj.ThrowResult(obj.Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_AbortProcess,obj.Pointer));
			obj.EventRecorder.LogEvent(Gbec.UID.Event_ProcessAborted);
			for V=obj.HostActions.values('cell').'
				V{1}.Abort();
			end
			obj.LogPrint('会话已放弃');
			if obj.SaveFile&&questdlg('是否保存现有数据？','实验已放弃','确定','取消','确定')~="取消"
				obj.SaveInformation;
			else
				warning("数据未保存");
			end
			obj.State=Gbec.UID.State_Idle;
			obj.CountdownExempt.delete;
		end
	end
end
function FprintfInCommandWindow(varargin)
if feature('SuppressCommandLineOutput')
	timer(StartDelay=0.1,TimerFcn=@(~,~)fprintf(varargin{:})).start;
else
	fprintf(varargin{:});
end
end
function SendMiao(Note,HttpRetryTimes,MiaoCode)
for a=0:HttpRetryTimes
	try
		%HttpRequest不支持中文，必须用webread
		webread(sprintf('http://miaotixing.com/trigger?id=%s&text=%s',MiaoCode,Note));
	catch ME
		if strcmp(ME.identifier,'MATLAB:webservices:UnknownHost')
			continue;
		else
			warning(ME.identifier,'%s',ME.message);
			break;
		end
	end
	break;
end
end