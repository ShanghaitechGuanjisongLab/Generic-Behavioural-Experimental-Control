%[text] 同时运行多个实验时，需要区分BOX1、Formal1等名称。BOX1对应串口，Formal1对应鼠，不同对应对象应有不同名称。
  %[control:button:0ad1]{"position":[1,2]}
if~(exist("BOX1","var")&&isa(BOX1,'Gbec.Server')&&BOX1.isvalid)
	BOX1=Gbec.Server;
end
%[text] # 在下方输入会话设置
%[text] 串口号
BOX1.Initialize('COM6',9600); %[output:9945f7a8]
if~(exist('Formal1','var')&&Formal1.IsValid&&Formal1.Server==BOX1)
	Formal1=Gbec.Formal(BOX1);
	Formal1.LogName='BOX1';
end
if Formal1.State~=Gbec.UID.State_Idle
	Gbec.Exception.Process_not_idle.Throw;
end
%[text] 选择要运行的会话
Formal1.SessionID=Gbec.UID.Session_AudioLearnWater;
SessionName=char(Formal1.SessionID);
%%
%[text] 鼠名和路径可以在实验运行中修改，注意修改鼠名后不会自动修改路径
Formal1.Mouse='假🐀';
Formal1.DateTime=datetime;
%[text] 是否要在每次实验（第一次除外）后监控行为曲线；若无需监控可设为false。若设为true，必须安装[统一实验分析作图](https://github.com/ShanghaitechGuanjisongLab/Unified-Experimental-Analysis-and-Figuring/releases)工具箱。
%[text] 示例代码根据鼠名自动生成路径，你也可以另行设定命名规则。
if true %[control:checkbox:9422]{"position":[4,8]} %[output:group:4bae0c3f]
	Filename=sprintf('D:\\张天夫\\%s.%s',Formal1.Mouse,SessionName(9:end)); %[output:9cf37413]
else
	Filename=sprintf('D:\\张天夫\\%s.%s.%s',Formal1.Mouse,char(Formal1.DateTime,'yyyyMMddHHmm'),SessionName(9:end));
end %[output:group:4bae0c3f]
Formal1.SavePath=strcat(Filename,'.行为.mat');
%%
%[text] 实验性功能，是否每个回合自动保存一次。此方法可以缓解意外崩溃的数据损失，但有一定性能代价。
if true %[control:checkbox:33cb]{"position":[4,8]}
	Formal1.TrialwiseSave=strcat(Filename,'.回合备份.mat');
else
	Formal1.TrialwiseSave='';
end
%%
%[text] 是否要在每次会话结束后展示事件记录图，如不设置则将此属性设为空；如设置，必须安装[统一实验分析作图](https://github.com/ShanghaitechGuanjisongLab/Unified-Experimental-Analysis-and-Figuring/releases)工具箱。
switch "不作图" %[control:dropdown:4ed9]{"position":[8,13]}
	case "默认作图"
		%此属性是一个元胞数组，分别代表要用于标志回合的事件、每个回合相对于标志事件的时间范围、要排除不作图的事件
		Formal1.TepArguments={["灯光亮","声音响"],seconds([-5,20]),'ExcludedEvents',["灯光灭","错失","命中","回合开始","声音停"]};
	case "自定义作图"
		%如果你使用自定义的作图代码，将TepArguments的第一个元胞中放置你的函数句柄。详见ExperimentWorker.TepArguments文档
		Formal1.TepArguments={@YourFunction,OtherArguments};
end
%%
%[text] 如果使用喵提醒服务，输入事件ID
if false %[control:checkbox:8731]{"position":[4,9]}
	Formal1.MiaoCode="tu9ijL8";

	%在实验结束前几个回合发送喵提醒？如果MiaoCode为空此设置不起作用
	Formal1.TrialsBeforeEndRemind=1; %[control:spinner:0d3c]{"position":[32,33]}
else
	Formal1.MiaoCode='';
end
%%
%[text] 检查周期，每重复这些回合数后，将发送提醒，警告实验员检查实验是否正常运行
Formal1.CheckCycle=40; %[control:spinner:5cf6]{"position":[20,22]}
%%
%[text] ## 主机动作
%[text] 可以用Arduino指挥主机执行一些无法用Arduino执行的动作，如拍摄视频、显示图像等。以下两个示例，可选采用，亦可编写自定义的主机动作，只需要实现[Gbec.IHostAction](<matlab:helpwin Gbec.IHostAction>)接口：
%[text] ### 视频拍摄
%[text] 此动作依赖 Image Acquisition Toolbox Support Package for OS Generic Video Interface。
%[text] 如果不拍视频，将if条件设为false即可。如果不清楚该怎么设置，先使用 Image Acquisition App 生成代码，得到所需参数。
if false %[control:checkbox:9fe7]{"position":[4,9]}
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
elseif Formal1.HostActions.isConfigured
	Formal1.HostActions(Gbec.UID.Host_StartRecord)=[];
end
%[text] 此例中，在Arduino端向串口发送UID.Host\_StartRecord即可开始拍摄。参见[Gbec.VideoRecord](<matlab:edit Gbec.VideoRecord>)
%%
%[text] ### 栅格图像
%[text] 如果不显示图像，将if条件设为false即可。
if false %[control:checkbox:320d]{"position":[4,9]}
	Formal1.HostActions{Gbec.UID.Host_GratingImage}=Gbec.GratingImage(CyclesPerWidth=[1,10],DurationRange=1);
elseif Formal1.HostActions.isConfigured
	Formal1.HostActions(Gbec.UID.Host_GratingImage)=[];
end
%[text] 此例中，在Arduino端向串口发送UID.Host\_GratingImage即可显示图像。参见[Gbec.GratingImage](<matlab:edit Gbec.GratingImage>)
%%
%[text] # 然后运行脚本，在命令行窗口中执行交互
Formal1.StartSession;
return;
  %[control:button:0ffc]{"position":[1,2]}
%%
%[text] # 实时控制命令
%%
  %[control:button:5cdf]{"position":[1,2]}
Formal1.PauseSession; %[output:7e9fa769]
%%
  %[control:button:364b]{"position":[1,2]}
Formal1.ContinueSession;
%%
  %[control:button:472e]{"position":[1,2]}
Formal1.AbortSession;
%%
  %[control:button:1fa9]{"position":[1,2]}
Formal1Info=Formal1.GetInformation
%%
%[text] 如果自动保存信息失败，可以再次尝试
  %[control:button:4711]{"position":[1,2]}
Formal1.SaveInformation;
%%
  %[control:button:4558]{"position":[1,2]}
Formal1.State
%%
  %[control:button:1b5e]{"position":[1,2]}
delete(BOX1) %[output:7f525406]

%[appendix]{"version":"1.0"}
%---
%[metadata:view]
%   data: {"layout":"inline","rightPanelPercent":40}
%---
%[control:button:0ad1]
%   data: {"label":"运行实验","run":"AllSections"}
%---
%[control:checkbox:9422]
%   data: {"defaultValue":true,"label":"复选框","run":"Nothing"}
%---
%[control:checkbox:33cb]
%   data: {"defaultValue":true,"label":"复选框","run":"Nothing"}
%---
%[control:dropdown:4ed9]
%   data: {"defaultValue":"\"不作图\"","itemLabels":["默认作图","自定义作图","不作图"],"items":["\"默认作图\"","\"自定义作图\"","\"不作图\""],"label":"下拉列表","run":"Nothing"}
%---
%[control:checkbox:8731]
%   data: {"defaultValue":true,"label":"复选框","run":"Nothing"}
%---
%[control:spinner:0d3c]
%   data: {"defaultValue":1,"label":"微调器","max":2,"min":1,"run":"Section","runOn":"ValueChanging","step":1}
%---
%[control:spinner:5cf6]
%   data: {"defaultValue":40,"label":"微调器","max":500,"min":1,"run":"Section","runOn":"ValueChanging","step":1}
%---
%[control:checkbox:9fe7]
%   data: {"defaultValue":true,"label":"复选框","run":"Nothing"}
%---
%[control:checkbox:320d]
%   data: {"defaultValue":true,"label":"复选框","run":"Nothing"}
%---
%[control:button:0ffc]
%   data: {"label":"运行实验","run":"AllSections"}
%---
%[control:button:5cdf]
%   data: {"label":"暂停会话","run":"Section"}
%---
%[control:button:364b]
%   data: {"label":"继续会话","run":"Section"}
%---
%[control:button:472e]
%   data: {"label":"放弃会话","run":"Section"}
%---
%[control:button:1fa9]
%   data: {"label":"获取信息","run":"Section"}
%---
%[control:button:4711]
%   data: {"label":"手动保存","run":"Section"}
%---
%[control:button:4558]
%   data: {"label":"查询状态","run":"Section"}
%---
%[control:button:1b5e]
%   data: {"label":"关闭串口","run":"Section"}
%---
%[output:9945f7a8]
%   data: {"dataType":"error","outputData":{"errorType":"runtime","text":"错误使用 <a href=\"matlab:matlab.lang.internal.introspective.errorDocCallback('serialport', 'C:\\Program Files\\MATLAB\\R2026a\\toolbox\\matlab\\serialport\\interface\\serialport.m', 121)\" style=\"font-weight:bold\">serialport<\/a> (<a href=\"matlab: opentoline('C:\\Program Files\\MATLAB\\R2026a\\toolbox\\matlab\\serialport\\interface\\serialport.m',121,0)\">第 121 行<\/a>)\n无法连接到端口 'COM6' 上的 serialport 设备。请确认设备已连接到该端口，该端口未被占用，并且设备支持所有 serialport 输入实参和形参值。\n请参阅 <a href=\"matlab: helpview('matlab', 'serialport_connectError')\">相关文档<\/a> 了解故障排除步骤。\n\n出错 <a href=\"matlab:matlab.lang.internal.introspective.errorDocCallback('Async_stream_IO.AsyncSerialStream\/SerialInitialize', 'C:\\Users\\vhtmf\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Async_stream_IO\\AsyncSerialStream.m', 92)\" style=\"font-weight:bold\">Async_stream_IO.AsyncSerialStream\/SerialInitialize<\/a> (<a href=\"matlab: opentoline('C:\\Users\\vhtmf\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Async_stream_IO\\AsyncSerialStream.m',92,0)\">第 92 行<\/a>)\n\t\t\t\tobj.Serial=serialport(Port,BaudRate);\n\n出错 <a href=\"matlab:matlab.lang.internal.introspective.errorDocCallback('Async_stream_IO.AsyncSerialStream', 'C:\\Users\\vhtmf\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Async_stream_IO\\AsyncSerialStream.m', 153)\" style=\"font-weight:bold\">Async_stream_IO.AsyncSerialStream<\/a> (<a href=\"matlab: opentoline('C:\\Users\\vhtmf\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Async_stream_IO\\AsyncSerialStream.m',153,0)\">第 153 行<\/a>)\n\t\t\tobj.SerialInitialize(Port,BaudRate);\n\n出错 <a href=\"matlab:matlab.lang.internal.introspective.errorDocCallback('Gbec.Server\/Initialize', 'C:\\Users\\vhtmf\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Gbec\\Server.m', 131)\" style=\"font-weight:bold\">Gbec.Server\/Initialize<\/a> (<a href=\"matlab: opentoline('C:\\Users\\vhtmf\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Gbec\\Server.m',131,0)\">第 131 行<\/a>)\n\t\t\t\t\tobj.AsyncStream=Async_stream_IO.AsyncSerialStream(varargin{:});"}}
%---
%[output:9cf37413]
%   data: {"dataType":"error","outputData":{"errorType":"runtime","text":"无法对 'sprintf' 进行索引。必须使用 end 运算符对现有变量进行索引。"}}
%---
%[output:7e9fa769]
%   data: {"dataType":"error","outputData":{"errorType":"runtime","text":"无法解析名称 'Formal1.PauseSession'。"}}
%---
%[output:7f525406]
%   data: {"dataType":"error","outputData":{"errorType":"runtime","text":"函数或变量 'BOX1' 无法识别。"}}
%---
