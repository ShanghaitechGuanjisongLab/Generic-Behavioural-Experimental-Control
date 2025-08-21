%[text] # 此脚本用于在实验前检查硬件设备是否正常工作
%[text] 所有检查之前先初始化
if~(exist("COM4","var")&&isa(BOX1,'Gbec.Server')) %[output:group:6eb52add]
	BOX1=Gbec.Server; %[output:30c65fa7]
end %[output:group:6eb52add]
BOX1.Initialize('COM4',9600);
if~(exist('Test1','var')&&Test1.Server==BOX1)
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
%[text] 检查喷气-单次
EW.OneEnterOneCheck(Gbec.UID.Test_AirPump,"按一次回车喷一次气，输入任意字符结束检查：");
%%
%[text] 检查喷气-多次
Test1.RepeatCheck(Gbec.UID.Test_AirPump);
%%
%[text] 主机动作测试。此处示例GratingImage的主机测试，可根据需要替换为其它类型测试
GI=Gbec.GratingImage.New(CyclesPerWidth=[1,10]);
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
%[text] ## 断开连接
clearvars BOX1; %[output:8ac58ea3] %[output:4158ff40] %[output:1e2198e2] %[output:7d7a402a] %[output:96ec5bb4] %[output:7b41eba7] %[output:65bbad88] %[output:1409d30f] %[output:884df68f]

%[appendix]{"version":"1.0"}
%---
%[metadata:view]
%   data: {"layout":"inline","rightPanelPercent":40}
%---
%[output:30c65fa7]
%   data: {"dataType":"text","outputData":{"text":"通用行为实验控制器v6.2.1 by 张天夫\n","truncated":false}}
%---
%[output:8ac58ea3]
%   data: {"dataType":"text","outputData":{"text":"测试开始（自动结束）\n","truncated":false}}
%---
%[output:4158ff40]
%   data: {"dataType":"text","outputData":{"text":"测试开始（自动结束）\n","truncated":false}}
%---
%[output:1e2198e2]
%   data: {"dataType":"text","outputData":{"text":"测试开始（手动结束）\n","truncated":false}}
%---
%[output:7d7a402a]
%   data: {"dataType":"text","outputData":{"text":"成功检测到触摸信号：131\n成功检测到触摸信号：132\n成功检测到触摸信号：133\n成功检测到触摸信号：134\n成功检测到触摸信号：135\n成功检测到触摸信号：136\n成功检测到触摸信号：137\n成功检测到触摸信号：138\n成功检测到触摸信号：139\n成功检测到触摸信号：140\n成功检测到触摸信号：141\n成功检测到触摸信号：142\n成功检测到触摸信号：143\n成功检测到触摸信号：144\n成功检测到触摸信号：145\n成功检测到触摸信号：146\n成功检测到触摸信号：147\n测试结束\n","truncated":false}}
%---
%[output:96ec5bb4]
%   data: {"dataType":"textualVariable","outputData":{"header":"包含以下字段的 struct:","name":"ans","value":"    CircularFrequency: 0.0155\n                Angle: 1.1779\n             Duration: Inf\n         InitialPhase: 0\n                Image: [4×1280×720 uint8]\n"}}
%---
%[output:7b41eba7]
%   data: {"dataType":"textualVariable","outputData":{"header":"包含以下字段的 struct:","name":"ans","value":"    CircularFrequency: 0.0370\n                Angle: 0.7083\n             Duration: Inf\n         InitialPhase: 0\n                Image: [4×1280×720 uint8]\n"}}
%---
%[output:65bbad88]
%   data: {"dataType":"textualVariable","outputData":{"header":"包含以下字段的 struct:","name":"ans","value":"    CircularFrequency: 0.0339\n                Angle: 0.6443\n             Duration: Inf\n         InitialPhase: 0\n                Image: [4×1280×720 uint8]\n"}}
%---
%[output:1409d30f]
%   data: {"dataType":"textualVariable","outputData":{"header":"包含以下字段的 struct:","name":"ans","value":"    CircularFrequency: 0.0051\n                Angle: 1.0529\n             Duration: Inf\n         InitialPhase: 0\n                Image: [4×1280×720 uint8]\n"}}
%---
%[output:884df68f]
%   data: {"dataType":"textualVariable","outputData":{"header":"包含以下字段的 struct:","name":"ans","value":"    CircularFrequency: 0.0324\n                Angle: -0.6458\n             Duration: Inf\n         InitialPhase: 0\n                Image: [4×1280×720 uint8]\n"}}
%---
