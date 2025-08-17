%[text] 获取并保存上次运行的会话信息到SaveFile文件。
function SaveInformation(obj)
obj.FeedDogIfActive;
DateTimes=table;
DateTimes.DateTime=obj.DateTime;
DateTimes.Mouse=categorical(obj.Mouse);
DateTimes.Metadata={obj.GetInformation};
Design=char(obj.SessionID);
Blocks=table;
Blocks.DateTime=obj.DateTime;
Blocks.Design=categorical(string(Design(strlength('Session_')+1:end)));
Query=obj.EventRecorder.GetTimeTable;
if ~isempty(Query)
	Query.Event=categorical(Gbec.LogTranslate(Query.Event));
end
Blocks.EventLog={Query};
Blocks.BlockIndex=0x1;
Blocks.BlockUID=0x001;
Trials=table;
Stimulus=obj.TrialRecorder.GetTimeTable;
NumTrials=height(Stimulus);
TrialIndex=(0x001:NumTrials)';
Trials.TrialUID=TrialIndex;
Trials.BlockUID(:)=0x001;
Trials.TrialIndex=TrialIndex;
Trials.Time=Stimulus.Time;
Stimulus=Gbec.LogTranslate(Stimulus.Event);
Untranslated=startsWith(Stimulus,'Trial_');
%split必须指定拆分维度，否则标量和向量行为不一致
SplitStimulus=split(Stimulus(Untranslated),'_',2);
Stimulus(Untranslated)=SplitStimulus(:,2);
Trials.Stimulus=categorical(Stimulus);
Version=Gbec.Version;
ShowFigure=false;
if isequaln(obj.MergeData,missing)
	save(obj.SavePath,'DateTimes','Blocks','Trials','Version','-nocompression');
else
	try
		DataSet=UniExp.DataSet.Merge(obj.MergeData,struct(DateTimes=DateTimes,Blocks=Blocks,Trials=Trials),OutputPath=obj.SavePath,MergeOutput=false);
		ShowFigure=true;
	catch ME
		warning(ME.identifier,'合并失败，单独保存');
		save(obj.SavePath,'DateTimes','Blocks','Trials','Version','-nocompression');
	end
end
SaveDirectory=fileparts(obj.SavePath);
disp("数据已保存到"+"<a href=""matlab:winopen('"+obj.SavePath+"');"">"+obj.SavePath+"</a> <a href=""matlab:cd('"+SaveDirectory+"');"">切换当前文件夹</a> <a href=""matlab:winopen('"+SaveDirectory+"');"">打开数据文件夹</a>");
if ShowFigure
	Query=sortrows(DataSet.TableQuery(["DateTime","EventLog"],Mouse=obj.Mouse),"DateTime");
	Fig=figure;
	plot(Query.DateTime,UniExp.EventLog2Performance(Query.EventLog));
	xlabel('DateTime');
	ylabel('Performance');
	title(obj.Mouse);
	[Directory,Filename]=fileparts(obj.SavePath);
	Fig.Visible='on';
	savefig(Fig,fullfile(Directory,Filename+".fig"));
end
end

%[appendix]{"version":"1.0"}
%---
