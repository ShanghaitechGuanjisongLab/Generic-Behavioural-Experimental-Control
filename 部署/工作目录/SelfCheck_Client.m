%[text] # 此脚本用于在实验前检查硬件设备是否正常工作
%[text] 此脚本可以与Formal实验并行运行，互不干扰。所有检查之前先初始化
if~(exist('BOX1','var')&&isa(BOX1,'Gbec.Server')&&BOX1.isvalid)
	BOX1=Gbec.Server;
end
BOX1.Initialize('COM6',9600);
if~(exist('Test1','var')&&isa(Test1,'Gbec.Test')&&Test1.IsValid&&Test1.Server==BOX1)
	Test1=Gbec.Test(BOX1);
end
%[text] 初始化完成后可以选择性地执行以下检查步骤
%%
%[text] 检查水管-多次水量检查
Test1.RepeatCheck(Gbec.UID.Test_WaterPump); %[output:8ca5ee60]
%%
%[text] 检查水管-单次水量检查
Test1.OneEnterOneCheck(Gbec.UID.Test_WaterPump,"按一次回车喷一次水，输入任意字符结束检查：");
%%
%[text] 电容重置
Test1.RepeatCheck(Gbec.UID.Test_CapacitorReset,1);
%%
%[text] 开始检查电容
Test1.StartCheck(Gbec.UID.Test_CapacitorMonitor);
%%
%[text] 停止检查电容
Test1.StopCheck;
%%
%[text] 检查蓝光
Test1.OneEnterOneCheck(Gbec.UID.Test_BlueLed,"按一次回车闪一次光，输入任意字符结束检查：");
%%
%[text] 检查CD1
Test1.OneEnterOneCheck(Gbec.UID.Test_CD1,"按一次回车打一个标，输入任意字符结束检查：");
%%
%[text] 检查声音
Test1.OneEnterOneCheck(Gbec.UID.Test_ActiveBuzzer,"按一次回车响一次声，输入任意字符结束检查：");
%%
%[text] 检查光遗传
Test1.OneEnterOneCheck(Gbec.UID.Test_Optogenetic,"按一次回车一次光遗传，输入任意字符结束检查：");
%%
%[text] 检查喷气-单次
Test1.OneEnterOneCheck(Gbec.UID.Test_AirPump,"按一次回车喷一次气，输入任意字符结束检查：");
%%
%[text] 检查喷气-多次
Test1.RepeatCheck(Gbec.UID.Test_AirPump);
%%
%[text] 主机动作测试。此处示例GratingImage的主机测试，可根据需要替换为其它类型测试
GI=Gbec.GratingImage(CyclesPerWidth=[1,10]);
Test1.HostAction=@(~)GI.Test;
Test1.OneEnterOneCheck(Gbec.UID.Test_HostAction,'按一次回车切一张图，输入任意字符结束测试：');
%%
%[text] 检查方波
Test1.OneEnterOneCheck(Gbec.UID.Test_SquareWave,"按一次回车一个方波，输入任意字符结束检查：");
%%
%[text] 检查随机闪烁
Test1.OneEnterOneCheck(Gbec.UID.Test_RandomFlash,"按一次回车一个随机闪烁，输入任意字符结束检查：");
%%
%[text] 检查低音
Test1.OneEnterOneCheck(Gbec.UID.Test_LowTone,"按一次回车一个低音，输入任意字符结束检查：");
%%
%[text] 检查高音
Test1.OneEnterOneCheck(Gbec.UID.Test_HighTone,"按一次回车一个高音，输入任意字符结束检查：");
%%
%[text] 检查喵提醒
Gbec.SendMiao('tu9ijL8');
%%
%[text] 关闭串口
delete(BOX1);

%[appendix]{"version":"1.0"}
%---
%[metadata:view]
%   data: {"layout":"inline","rightPanelPercent":40}
%---
%[output:8ca5ee60]
%   data: {"dataType":"error","outputData":{"errorType":"runtime","text":"错误使用 <a href=\"matlab:matlab.lang.internal.introspective.errorDocCallback('Gbec.Test\/RepeatCheck', 'D:\\Users\\张天夫\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Gbec\\Test.m', 64)\" style=\"font-weight:bold\">Gbec.Test\/RepeatCheck<\/a> (<a href=\"matlab: opentoline('D:\\Users\\张天夫\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Gbec\\Test.m',64,0)\">第 64 行<\/a>)\n参数 'NumTimes' 的默认值无效。 值必须为标量。"}}
%---
