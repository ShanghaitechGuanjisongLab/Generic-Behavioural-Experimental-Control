%[text] 获取并保存上次运行的会话信息到SaveFile文件。
function SaveInformation(obj)
obj.Server.FeedDogIfActive;
SP=obj.SavePath;
if isempty(obj.oTrialwiseSave)
	[DateTimes,Blocks]=obj.SessionMeta;
	DateTimes.Metadata={obj.GetInformation};
	Blocks.EventLog=obj.EventLog;
	Trials=table;
	Stimulus=obj.TrialRecorder.GetTimeTable;
	NumTrials=height(Stimulus);
	if NumTrials
		TrialIndex=(0x001:NumTrials)';
		Trials.TrialUID=TrialIndex;
		Trials.BlockUID(:)=0x001;
		Trials.TrialIndex=TrialIndex;
		Trials.Time=Stimulus.Time;
		Stimulus=Gbec.LogTranslate(Stimulus.Event);
		Untranslated=startsWith(Stimulus,'Trial_');
		%split必须指定拆分维度，否则标量和向量行为不一致
		SplitStimuli=Stimulus(Untranslated);
		try
			SplitStimuli=split(SplitStimuli,'_',2);
			Stimulus(Untranslated)=SplitStimuli(:,2);
		catch ME
			if ME.identifier=="MATLAB:string:MustHaveSameNumberOf"
				for S=1:numel(SplitStimuli)
					Splitted=split(SplitStimuli(S),"_");
					SplitStimuli(S)=join(Splitted(2:end),"_");
				end
				Stimulus(Untranslated)=SplitStimuli;
			else
				ME.rethrow;
			end
		end
		Trials.Stimulus=categorical(Stimulus);
	end
	Version=Gbec.Version;
	%不能在保存阶段询问用户是否覆盖，因为这可能会干扰其它运行中的实验的消息输出，因此覆盖可行性必须在SavePath时确认。
	if obj.OverwriteExisting%这个属性应当在设置SavePath时一同被设置
		%文件不存在或用户确认覆盖的情况都可以安全覆盖
		save(SP,'DateTimes','Blocks','Trials','Version');
	else
		%不覆盖只可能是文件存在且SavePath已尝试过可以UniExp合并。文件存在但不能合并的情形已经在设置SavePath时被拒绝了。
		DataSet=UniExp.DataSet.Merge(SP,struct(DateTimes=DateTimes,Blocks=Blocks,Trials=Trials));
		save(SP,'DataSet','Version');
	end
else
	Blocks=obj.oTrialwiseSave.Blocks;
	DateTimes=obj.oTrialwiseSave.DateTimes;

	%不能在会话开始前获取信息。也不能在刚开始后获取，因为会话可能是瞬时的，此时会话会在GetInformation之前结束并触发SaveInformation导致错误
	DateTimes.Metadata={obj.GetInformation};

	Blocks.EventLog={obj.EventLog};
	obj.oTrialwiseSave.Blocks=Blocks;
	obj.oTrialwiseSave.DateTimes=DateTimes;
	if obj.OverwriteExisting%这个属性应当在设置SavePath时一同被设置
		%文件不存在或用户确认覆盖的情况都可以安全覆盖
		copyfile(obj.oTrialwiseSave.Properties.Source,SP,'f');
	else
		%不覆盖只可能是文件存在且SavePath已尝试过可以UniExp合并。文件存在但不能合并的情形已经在设置SavePath时被拒绝了。
		DataSet=UniExp.DataSet.Merge(SP,obj.oTrialwiseSave.Properties.Source);
		MATLAB.General.Save(SP,DataSet,Version=Gbec.Version);
	end
end
[SaveDirectory,Filename]=fileparts(SP);
obj.LogPrint("数据已保存到<a href=""matlab:load('%s');"">%s</a> <a href=""matlab:cd('%s');"">切换当前文件夹</a> <a href=""matlab:winopen('%s');"">打开数据文件夹</a>\n",SP,SP,SaveDirectory,SaveDirectory);
if~obj.OverwriteExisting
	Blocks=sortrows(DataSet.TableQuery(["DateTime","EventLog"],Mouse=obj.Mouse),"DateTime");
	Fig=figure;
	plot(Blocks.DateTime,UniExp.EventLog2Performance(Blocks.EventLog));
	xlabel('DateTime');
	ylabel('Performance');
	title(obj.Mouse);
	Fig.Visible='on';
	savefig(Fig,fullfile(SaveDirectory,Filename+".fig"));
end
if ~isempty(obj.TepArguments)
	if isa(obj.TepArguments{1},'function_handle')
		obj.TepArguments{1}(Blocks,obj.TepArguments{2:end});
	else
		Fig=figure;
		UniExp.TrialwiseEventPlot(Blocks.EventLog{end},obj.TepArguments{:});
		title(obj.Mouse);
		savefig(Fig,fullfile(SaveDirectory,sprintf('%s.%s.fig',Filename,char(obj.DateTime,'yyyyMMddHHmm'))));
	end
end
end

%[appendix]{"version":"1.0"}
%---
