%[text] 将本文件中的“BOX1”替换成任何自定义名称，可以同时在工作区中存在多个Server和Formal对象，以在本MATLAB会话中同时执行多个实验会话，甚至从同一个COM口（开发板）同时运行多个实验
if~(exist("BOX1","var")&&isa(BOX1,'Gbec.Server'))
	BOX1=Gbec.Server;
end
%[text] # 在下方输入会话设置
%[text] 串口号
BOX1.Initialize('BOX1',9600);
if~(exist('Formal3','var')&&Formal3.Server==BOX1)
	Formal3=Gbec.Formal(BOX1);
	Formal3.LogName='BOX1';
end
%[text] 选择要运行的会话
Formal3.SessionUID=Gbec.UID.Session_AudioWater;
SessionName=char(Formal3.SessionUID);
%[text] 设置实验基本信息
Formal3.Mouse='yqn0204';
Formal3.DateTime=datetime;
%[text] 是否要在每次实验（第一次除外）后监控行为曲线；若无需监控可设为false。若设为true，必须安装[统一实验分析作图](https://github.com/ShanghaitechGuanjisongLab/Unified-Experimental-Analysis-and-Figuring/releases)工具箱。
if true
	Filename=sprintf('D:\\张天夫\\%s.%s',Formal3.Mouse,SessionName(9:end));
else
	Filename=sprintf('D:\\张天夫\\%s.%s.%s',Formal3.Mouse,char(Formal3.DateTime,'yyyyMMddHHmm'),SessionName(9:end));
end
Formal3.SavePath=strcat(Filename,'.行为.UniExp.mat');
%[text] 是否要在每次会话结束后展示事件记录图，如不设置则将此属性设为空；如设置，必须安装[统一实验分析作图](https://github.com/ShanghaitechGuanjisongLab/Unified-Experimental-Analysis-and-Figuring/releases)工具箱。
%[text] 此属性是一个元胞数组，分别代表要用于标志回合的事件、每个回合相对于标志事件的时间范围、要排除不作图的事件
Formal3.TepArguments={["灯光亮","声音响"],seconds([-5,20]),'ExcludedEvents',["灯光灭","错失","命中","回合开始","声音停"]};
%[text] 如果你使用自定义的作图代码，将TepArguments的第一个元胞中放置你的函数句柄。详见ExperimentWorker.TepArguments文档
% Formal3.TepArguments={@YourFunction,OtherArguments...};
%[text] 如果使用喵提醒服务，输入事件ID；如果不使用，设为空字符串""
Formal3.MiaoCode="";
%[text] 在实验结束前几个回合发送喵提醒？如果MiaoCode为空此设置不起作用
Formal3.TrialsBeforeEndRemind=1;
%[text] 检查周期，每重复这些回合数后，将发送提醒，警告实验员检查实验是否正常运行
Formal3.CheckCycle=50;
%[text] ## 主机动作
%[text] 可以用Arduino指挥主机执行一些无法用Arduino执行的动作，如拍摄视频、显示图像等。以下两个示例，可选采用，亦可编写自定义的主机动作，只需要实现[Gbec.IHostAction](<matlab:helpwin Gbec.IHostAction>)接口：
%[text] ### 视频拍摄
%[text] 此动作依赖 Image Acquisition Toolbox Support Package for OS Generic Video Interface。
%[text] 如果不拍视频，将if条件设为false即可。如果不清楚该怎么设置，先使用 Image Acquisition App 生成代码，得到所需参数。
if false
	VideoArguments={'winvideo',1,'YUY2_640x360'};%选择正确的设备和格式
	try
		VideoInput=videoinput(VideoArguments{:});
	catch
		imaqreset
		VideoInput=videoinput(VideoArguments{:});
	end
	VideoInput.DiskLogger=VideoWriter(strcat(Filename,'.mp4'),'MPEG-4'); %选择视频保存路径
	VideoInput.TriggerRepeat=Inf;%一般不应修改，除非提前知道回合数
	VideoInput.FramesPerTrigger=200; %拍摄帧数，通常大于回合关键部分的时长×帧速率。如果全程拍摄，设为Inf
	VideoInput.LoggingMode='disk'; %记录模式，一般无需修改。如果不写出到磁盘，请设为'memory'。
	triggerconfig(VideoInput,'manual');%如果全程拍摄，设为immediate；设为manual则只拍回合开始后一段时间
	Formal3.HostActions{Gbec.UID.Host_StartRecord}=Gbec.VideoRecord(VideoInput);
end
%[text] 此例中，在Arduino端向串口发送UID.Host\_StartRecord即可开始拍摄。参见[Gbec.VideoRecord](<matlab:edit Gbec.VideoRecord>)
%[text] ### 栅格图像
%[text] 如果不显示图像，将if条件设为false即可。
if false
	Formal3.HostActions{Gbec.UID.Host_GratingImage}=Gbec.GratingImage(CyclesPerWidth=[1,10]);
end
%[text] 此例中，在Arduino端向串口发送UID.Host\_GratingImage即可显示图像。参见[Gbec.GratingImage](<matlab:edit Gbec.GratingImage>)
%[text] # 然后运行脚本，在命令行窗口中执行交互
Formal3.StartSession;
return;
%%
%[text] # 实时控制命令
%%
%[text] 暂停会话
Formal3.PauseSession;
%%
%[text] 继续会话
Formal3.ContinueSession;
%%
%[text] 放弃会话
Formal3.AbortSession;
%%
%[text] 获取信息
Formal3.GetInformation
%%
%[text] 查询状态
Formal3.State
%%
%[text] 关闭串口
clearvars Formal3;

%[appendix]{"version":"1.0"}
%---
%[metadata:view]
%   data: {"layout":"inline","rightPanelPercent":40}
%---
