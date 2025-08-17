%[text] 开始会话
%[text] 此方法会先检查指定的文件保存位置是否可访问，询问用户确认文件保存是否正确，是否覆盖已存在的文件，然后才会开始会话。
function StartSession(obj)
obj.Server.FeedDogIfActive;
if obj.State~=Gbec.UID.State_Idle
	Gbec.Exception.Process_not_idle.Throw;
end
if questdlg(strcat('将保存为：',obj.SavePath),'确认？','确定','取消','确定')~="确定"
	obj.LogPrint("取消会话");
	return;
end
if isfile(obj.SavePath)
	if isempty(which('UniExp.Version'))
		if questdlg('未找到统一实验分析作图工具箱，无法合并已存在的文件','是否覆盖？','是','否','否')=="否"
			obj.LogPrint("取消会话");
			return;
		end
	else
		obj.LogPrint("目标文件已存在，将尝试合并");
		try
			obj.MergeData=UniExp.DataSet(obj.SavePath);
			[Directory,Filename]=fileparts(obj.SavePath);
			movefile(obj.SavePath,fullfile(Directory,Filename+".将合并.mat"));
		catch ME
			if questdlg('合并失败，是否要覆盖文件？',ME.identifier,'是','否','否')=="否"
				obj.FeedDog;
				obj.LogPrint("取消会话");
				return;
			else
				obj.MergeData=missing;
			end
		end
	end
else
	obj.MergeData=missing;
end
Fid=fopen(obj.SavePath,'w');
if Fid==-1
	mkdir(fileparts(obj.SavePath));
	Fid=fopen(obj.SavePath,'w');
end
if Fid==-1
	Gbec.Exception.No_right_to_write_SavePath.Throw;
end
fclose(Fid);
if ~isempty(obj.VideoInput)
	%这一步很慢，必须优先执行完再启动Arduino。
	preview(obj.VideoInput);
	start(obj.VideoInput);
	waitfor(obj.VideoInput,'Running','on');
end
Return=obj.Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_StartModule,obj.Pointer,obj.SessionID,0x001);
Gbec.Process.ThrowResult(Return(1));
obj.CountdownExempt=Gbec.CountdownExempt_(obj.Server);
obj.DesignedNumTrials=typecast(Return(2:end),'uint16');
obj.EventRecorder.Reset;
obj.TrialRecorder.Reset;
obj.PreciseRecorder.Clear;
obj.State=Gbec.UID.State_Running;
obj.LogPrint('会话开始，回合总数：%u，将保存为：%s\n',obj.DesignedNumTrials,obj.SavePath);
end

%[appendix]{"version":"1.0"}
%---
