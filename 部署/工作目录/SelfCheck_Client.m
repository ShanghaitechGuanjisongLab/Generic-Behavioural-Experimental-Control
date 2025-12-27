%[text] # 此脚本用于在实验前检查硬件设备是否正常工作
%[text] 所有检查之前先初始化
if~(exist('BOX1','var')&&isa(BOX1,'Gbec.Server')&&BOX1.isvalid) %[output:group:3b877071]
	BOX1=Gbec.Server; %[output:723cbcc2]
end %[output:group:3b877071]
BOX1.Initialize('COM6',9600);
if~(exist('Test1','var')&&isa(Test1,'Gbec.Test')&&Test1.IsValid&&Test1.Server==BOX1)
	Test1=Gbec.Test(BOX1);
end
%[text] 初始化完成后可以选择性地执行以下检查步骤
%%
%[text] 检查水管-多次水量检查
Test1.RepeatCheck(Gbec.UID.Test_WaterPump);
%%
%[text] 检查水管-单次水量检查
Test1.OneEnterOneCheck(Gbec.UID.Test_WaterPump,"按一次回车喷一次水，输入任意字符结束检查：");
%%
%[text] 电容重置
Test1.RepeatCheck(Gbec.UID.Test_CapacitorReset,1);
%%
%[text] 开始检查电容
Test1.StartCheck(Gbec.UID.Test_CapacitorMonitor); %[output:599ca1f3]
%%
%[text] 停止检查电容
Test1.StopCheck; %[output:55383149]
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
Test1.OneEnterOneCheck(Gbec.UID.Test_HighTone,"按一次回车一个高音，输入任意字符结束检查："); %[output:296e83dd] %[output:92e16717] %[output:4656371b] %[output:641688bd] %[output:815a477f] %[output:6153ec73] %[output:5f15b136]
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
%[output:723cbcc2]
%   data: {"dataType":"text","outputData":{"text":"通用行为实验控制器v8.0.1 by 张天夫\n","truncated":false}}
%---
%[output:599ca1f3]
%   data: {"dataType":"text","outputData":{"text":"Test_CapacitorMonitor……\n","truncated":false}}
%---
%[output:55383149]
%   data: {"dataType":"text","outputData":{"text":"测试结束\n","truncated":false}}
%---
%[output:296e83dd]
%   data: {"dataType":"text","outputData":{"text":"通用行为实验控制v6.5.0已发布，<a href=\"https:\/\/github.com\/ShanghaitechGuanjisongLab\/Generic-Behavioural-Experimental-Control\/releases\">立即更新<\/a>\n通用行为实验控制器v7.0.0 by 张天夫\n","truncated":false}}
%---
%[output:92e16717]
%   data: {"dataType":"text","outputData":{"text":"Test_WaterPump×3……\n","truncated":false}}
%---
%[output:4656371b]
%   data: {"dataType":"text","outputData":{"text":"Test_CapacitorReset×1……\n","truncated":false}}
%---
%[output:641688bd]
%   data: {"dataType":"text","outputData":{"text":"Test_CapacitorMonitor……\n","truncated":false}}
%---
%[output:815a477f]
%   data: {"dataType":"text","outputData":{"text":"测试结束\n","truncated":false}}
%---
%[output:6153ec73]
%   data: {"dataType":"warning","outputData":{"text":"警告: Async_stream_IO:Exception:Unlistened_port_received_message：Port 16, MessageSize 0"}}
%---
%[output:5f15b136]
%   data: {"dataType":"error","outputData":{"errorType":"runtime","text":"错误使用 <a href=\"matlab:matlab.lang.internal.introspective.errorDocCallback('Gbec.Process\/ThrowResult', 'D:\\Users\\Administrator\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Gbec\\Process.m', 26)\" style=\"font-weight:bold\">Gbec.Process\/ThrowResult<\/a> (<a href=\"matlab: opentoline('D:\\Users\\Administrator\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Gbec\\Process.m',26,0)\">第 26 行<\/a>)\nGbec:UID:Exception_InvalidProcess\n\n出错 <a href=\"matlab:matlab.lang.internal.introspective.errorDocCallback('Gbec.Test\/OneEnterOneCheck', 'D:\\Users\\Administrator\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Gbec\\Test.m', 31)\" style=\"font-weight:bold\">Gbec.Test\/OneEnterOneCheck<\/a> (<a href=\"matlab: opentoline('D:\\Users\\Administrator\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Gbec\\Test.m',31,0)\">第 31 行<\/a>)\n\t\t\t\tobj.ThrowResult(obj.Server.AsyncStream.SyncInvoke(Gbec.UID.PortA_StartModule,obj.Pointer,TestID,0x001));\n    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"}}
%---
