%[text] 开始会话
%[text] 此方法会先检查指定的文件保存位置是否可访问，询问用户确认文件保存是否正确，是否覆盖已存在的文件，然后才会开始会话。
function StartSession(obj)
obj.Server.FeedDogIfActive;
if obj.State~=Gbec.UID.State_Idle
	Gbec.Exception.Process_not_idle.Throw;
end
if obj.OverwriteExisting
	OverwriteOrMerge='覆盖掉';
else
	OverwriteOrMerge='合并到';
end
if questdlg(sprintf('%s：数据将%s：%s',obj.LogName,OverwriteOrMerge,obj.SavePath),'确认？','确定','取消','确定')~="确定"
	Gbec.Exception.User_canceled_operation.Throw('如果希望改变覆盖/合并行为，请重新设置SavePath');
end
if ~isempty(obj.VideoInput)
	%这一步很慢，必须优先执行完再启动Arduino。
	preview(obj.VideoInput);
	start(obj.VideoInput);
	waitfor(obj.VideoInput,'Running','on');
end
Return=obj.Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_StartModule,obj.Pointer,obj.SessionID,0x001);
obj.ThrowResult(Return(1));
obj.CountdownExempt=Gbec.CountdownExempt_(obj.Server);
obj.DesignedNumTrials=typecast(Return(2:end),'uint16');
obj.EventRecorder.Reset;
obj.TrialRecorder.Reset;
obj.State=Gbec.UID.State_Running;
obj.LogPrint('会话开始，回合总数：%u，将保存为：%s\n',obj.DesignedNumTrials,obj.SavePath);
end

%[appendix]{"version":"1.0"}
%---
