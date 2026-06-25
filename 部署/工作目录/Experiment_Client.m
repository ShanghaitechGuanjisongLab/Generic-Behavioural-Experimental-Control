%[text] 将本文件中的“BOX1”和“Formal1”替换成任何自定义名称，可以同时在工作区中存在多个Server和Formal对象，以在本MATLAB会话中同时执行多个实验会话，甚至从同一个COM口（开发板）同时运行多个实验
if~(exist("BOX1","var")&&isa(BOX1,'Gbec.Server')&&BOX1.isvalid) %[output:group:3dcd8677]
	BOX1=Gbec.Server; %[output:3e7a14c5]
end %[output:group:3dcd8677]
%[text] # 在下方输入会话设置
%[text] 串口号
BOX1.Initialize('COM6',9600);
if~(exist('Formal1','var')&&Formal1.IsValid&&Formal1.Server==BOX1)
	Formal1=Gbec.Formal(BOX1);
	Formal1.LogName='BOX1';
end
if Formal1.State~=Gbec.UID.State_Idle
	Gbec.Exception.Process_not_idle.Throw;
end
if Formal1.State~=Gbec.UID.State_Idle
	Gbec.Exception.Process_not_idle.Throw;
end
%[text] 选择要运行的会话
Formal1.SessionID=Gbec.UID.Session_AudioLearnWater;
SessionName=char(Formal1.SessionID);
%[text] 设置实验基本信息
Formal1.Mouse='假🐀';
%[text] 是否要在每次实验（第一次除外）后监控行为曲线；若无需监控可设为false。若设为true，必须安装[统一实验分析作图](https://github.com/ShanghaitechGuanjisongLab/Unified-Experimental-Analysis-and-Figuring/releases)工具箱。
if false
	Filename=sprintf('D:\\张天夫\\%s.%s',Formal1.Mouse,SessionName(9:end));
    Formal1.DateTime=missing;
else
    Formal1.DateTime=datetime;
	Filename=sprintf('D:\\张天夫\\%s.%s.%s',Formal1.Mouse,char(Formal1.DateTime,'yyyyMMddHHmm'),SessionName(9:end));
end
Formal1.SavePath=strcat(Filename,'.行为.mat');
%[text] 实验性功能，是否每个回合自动保存一次。此方法可以缓解意外崩溃的数据损失，但有一定性能代价。如果不使用此功能，设为空。
Formal1.TrialwiseSave=strcat(Filename,'.回合备份.mat');
%[text] 是否要在每次会话结束后展示事件记录图，如不设置则将此属性设为空；如设置，必须安装[统一实验分析作图](https://github.com/ShanghaitechGuanjisongLab/Unified-Experimental-Analysis-and-Figuring/releases)工具箱。
%[text] 此属性是一个元胞数组，分别代表要用于标志回合的事件、每个回合相对于标志事件的时间范围、要排除不作图的事件
% Formal1.TepArguments={["灯光亮","声音响"],seconds([-5,20]),'ExcludedEvents',["灯光灭","错失","命中","回合开始","声音停"]};
%[text] 如果你使用自定义的作图代码，将TepArguments的第一个元胞中放置你的函数句柄。详见ExperimentWorker.TepArguments文档
% Formal1.TepArguments={@YourFunction,OtherArguments…};
%[text] 如果使用喵提醒服务，输入事件ID；如果不使用，设为空字符串""
Formal1.MiaoCode="tu9ijL8";
%[text] 在实验结束前几个回合发送喵提醒？如果MiaoCode为空此设置不起作用
Formal1.TrialsBeforeEndRemind=1;
%[text] 检查周期，每重复这些回合数后，将发送提醒，警告实验员检查实验是否正常运行
Formal1.CheckCycle=40;
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
	Formal1.HostActions{Gbec.UID.Host_StartRecord}=Gbec.VideoRecord(VideoInput);
end
%[text] 此例中，在Arduino端向串口发送UID.Host\_StartRecord即可开始拍摄。参见[Gbec.VideoRecord](<matlab:edit Gbec.VideoRecord>)
%[text] ### 栅格图像
%[text] 如果不显示图像，将if条件设为false即可。
if false
	Formal1.HostActions{Gbec.UID.Host_GratingImage}=Gbec.GratingImage(CyclesPerWidth=[1,10],DurationRange=1);
end
%[text] 此例中，在Arduino端向串口发送UID.Host\_GratingImage即可显示图像。参见[Gbec.GratingImage](<matlab:edit Gbec.GratingImage>)
%[text] # 然后运行脚本，在命令行窗口中执行交互
Formal1.StartSession; %[output:1ebca54f]
if ismissing(Formal1.DateTime)
    Formal1.DateTime=datetime;
end
return;
%%
%[text] # 实时控制命令
%%
%[text] 暂停会话
Formal1.PauseSession;
%%
%[text] 继续会话
Formal1.ContinueSession;
%%
%[text] 放弃会话
Formal1.AbortSession;
%%
%[text] 获取信息
Formal1Info=Formal1.GetInformation
%%
%[text] 如果自动保存信息失败，可以再次尝试手动保存
Formal1.SaveInformation;
%%
%[text] 查询状态
Formal1.State
%%
%[text] 关闭串口
delete(BOX1)

%[appendix]{"version":"1.0"}
%---
%[metadata:view]
%   data: {"layout":"inline","rightPanelPercent":40}
%---
%[output:3e7a14c5]
%   data: {"dataType":"text","outputData":{"text":"通用行为实验控制器v8.2.0 by 张天夫\n","truncated":false}}
%---
%[output:1ebca54f]
%   data: {"dataType":"text","outputData":{"text":"\nBOX1：会话开始，回合总数：50，将保存为：D:\\张天夫\\假🐀.202606251408.AudioLearnWater.行为.mat\n","truncated":false}}
%---
