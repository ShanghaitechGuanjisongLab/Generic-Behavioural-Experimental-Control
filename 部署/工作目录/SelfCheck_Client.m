%[text] # 此脚本用于在实验前检查硬件设备是否正常工作
%[text] 此脚本可以与Formal实验并行运行，互不干扰。所有检查之前先初始化
if~(exist('BOX1','var')&&isa(BOX1,'Gbec.Server')&&BOX1.isvalid)
	BOX1=Gbec.Server;
end
BOX1.Initialize('COM6',9600);
if~(exist('Test1','var')&&isa(Test1,'Gbec.Test')&&Test1.IsValid&&Test1.Server==BOX1)
	Test1=Gbec.Test(BOX1);
end
  %[control:button:2e71]{"position":[1,2]}
%[text] 初始化完成后可以选择性地执行以下检查步骤
%%
  %[control:button:2040]{"position":[1,2]}
Test1.RepeatCheck(Gbec.UID.Test_WaterPump,1); %[control:slider:04d2]{"position":[43,44]} %[output:8ca5ee60]
%%
  %[control:button:90e8]{"position":[1,2]}
Test1.OneEnterOneCheck(Gbec.UID.Test_WaterPump,"按一次回车喷一次水，输入任意字符结束检查：");
%%
  %[control:button:6d47]{"position":[1,2]}
Test1.RepeatCheck(Gbec.UID.Test_CapacitorReset,1);
%%
if false %[control:statebutton:87b9]{"position":[4,9]} %[output:group:0f1b8064]
	Test1.StartCheck(Gbec.UID.Test_CapacitorMonitor);
else
	Test1.StopCheck; %[output:001ff5d3]
end %[output:group:0f1b8064]
%%
  %[control:button:0a8d]{"position":[1,2]}
Test1.OneEnterOneCheck(Gbec.UID.Test_BlueLed,"按一次回车闪一次光，输入任意字符结束检查：");
%%
  %[control:button:6e82]{"position":[1,2]}
Test1.OneEnterOneCheck(Gbec.UID.Test_CD1,"按一次回车打一个标，输入任意字符结束检查：");
%%
  %[control:button:349c]{"position":[1,2]}
Test1.OneEnterOneCheck(Gbec.UID.Test_ActiveBuzzer,"按一次回车响一次声，输入任意字符结束检查：");
%%
  %[control:button:6a2c]{"position":[1,2]}
Test1.OneEnterOneCheck(Gbec.UID.Test_Optogenetic,"按一次回车一次光遗传，输入任意字符结束检查：");
%%
  %[control:button:63b0]{"position":[1,2]}
Test1.OneEnterOneCheck(Gbec.UID.Test_AirPump,"按一次回车喷一次气，输入任意字符结束检查：");
%%
  %[control:button:858b]{"position":[1,2]}
Test1.RepeatCheck(Gbec.UID.Test_AirPump,1); %[control:slider:2a03]{"position":[41,42]}
%%
%[text] 此处示例GratingImage的主机测试，可根据需要替换为其它类型测试
  %[control:button:6ed9]{"position":[1,2]}
GI=Gbec.GratingImage(CyclesPerWidth=[1 11]); %[control:rangeslider:4dfc]{"position":[37,43]}
Test1.HostAction=@(~)GI.Test;
Test1.OneEnterOneCheck(Gbec.UID.Test_HostAction,'按一次回车切一张图，输入任意字符结束测试：');
%%
  %[control:button:29cb]{"position":[1,2]}
Test1.OneEnterOneCheck(Gbec.UID.Test_SquareWave,"按一次回车一个方波，输入任意字符结束检查：");
%%
  %[control:button:525e]{"position":[1,2]}
Test1.OneEnterOneCheck(Gbec.UID.Test_RandomFlash,"按一次回车一个随机闪烁，输入任意字符结束检查：");
%%
  %[control:button:9af6]{"position":[1,2]}
Test1.OneEnterOneCheck(Gbec.UID.Test_LowTone,"按一次回车一个低音，输入任意字符结束检查：");
%%
  %[control:button:2537]{"position":[1,2]}
Test1.OneEnterOneCheck(Gbec.UID.Test_HighTone,"按一次回车一个高音，输入任意字符结束检查：");
%%
  %[control:button:24b2]{"position":[1,2]}
Gbec.SendMiao('tu9ijL8');
%%
  %[control:button:3eb3]{"position":[1,2]}
delete(BOX1);

%[appendix]{"version":"1.0"}
%---
%[metadata:view]
%   data: {"layout":"inline","rightPanelPercent":40}
%---
%[control:button:2e71]
%   data: {"label":"初始化","run":"Section"}
%---
%[control:button:2040]
%   data: {"label":"检查水管-多次水量检查","run":"Section"}
%---
%[control:slider:04d2]
%   data: {"defaultValue":1,"label":"滑块","max":500,"min":1,"run":"Nothing","runOn":"ValueChanging","step":1}
%---
%[control:button:90e8]
%   data: {"label":"检查水管-单次水量检查","run":"Section"}
%---
%[control:button:6d47]
%   data: {"label":"电容重置","run":"Section"}
%---
%[control:statebutton:87b9]
%   data: {"defaultValue":false,"label":"检查电容","run":"Section"}
%---
%[control:button:0a8d]
%   data: {"label":"检查蓝光","run":"Section"}
%---
%[control:button:6e82]
%   data: {"label":"检查CD1","run":"Section"}
%---
%[control:button:349c]
%   data: {"label":"检查声音","run":"Section"}
%---
%[control:button:6a2c]
%   data: {"label":"检查光遗传","run":"Section"}
%---
%[control:button:63b0]
%   data: {"label":"检查喷气-单次","run":"Section"}
%---
%[control:button:858b]
%   data: {"label":"检查喷气-多次","run":"Section"}
%---
%[control:slider:2a03]
%   data: {"defaultValue":1,"label":"滑块","max":500,"min":1,"run":"Nothing","runOn":"ValueChanging","step":1}
%---
%[control:button:6ed9]
%   data: {"label":"主机动作测试","run":"Section"}
%---
%[control:rangeslider:4dfc]
%   data: {"defaultValue":"[1 10]","label":"范围滑块","max":20,"min":1,"run":"Nothing","runOn":"ValueChanging","step":1}
%---
%[control:button:29cb]
%   data: {"label":"检查方波","run":"Section"}
%---
%[control:button:525e]
%   data: {"label":"检查随机闪烁","run":"Section"}
%---
%[control:button:9af6]
%   data: {"label":"检查低音","run":"Section"}
%---
%[control:button:2537]
%   data: {"label":"检查高音","run":"Section"}
%---
%[control:button:24b2]
%   data: {"label":"检查喵提醒","run":"Section"}
%---
%[control:button:3eb3]
%   data: {"label":"关闭串口","run":"Section"}
%---
%[output:8ca5ee60]
%   data: {"dataType":"error","outputData":{"errorType":"runtime","text":"无法解析名称 'Test1.RepeatCheck'。"}}
%---
%[output:001ff5d3]
%   data: {"dataType":"text","outputData":{"text":"\n2026-06-28 10:52:32 命中：101\n\n2026-06-28 10:52:32 命中：102\n\n2026-06-28 10:52:32 命中：103\n\n2026-06-28 10:52:32 命中：104\n\n2026-06-28 10:52:32 命中：105\n\n2026-06-28 10:52:32 命中：106\n\n2026-06-28 10:52:32 命中：107\n\n2026-06-28 10:52:32 命中：108\n\n2026-06-28 10:52:32 命中：109\n\n2026-06-28 10:52:32 命中：110\n\n2026-06-28 10:52:32 命中：111\n\n2026-06-28 10:52:32 命中：112\n\n2026-06-28 10:52:32 命中：113\n\n2026-06-28 10:52:32 命中：114\n\n2026-06-28 10:52:32 命中：115\n测试结束\n","truncated":false}}
%---
