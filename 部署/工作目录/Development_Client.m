%[text] # 此脚本用于Arduino端相关的配置
  %[control:button:17ff]{"position":[1,2]}
winopen(fullfile(userpath,'+Gbec\Gbec\Gbec.ino')); %[output:604529a8]
%[text] 执行Arduino端代码的修改（如引脚号、UID、实验设计等）和上传：
%[text] 主要需要根据硬件排布和实验设计，修改ExperimentDesign.cpp和UID.hpp，其它文件一般无需修改，修改方法在文件中有详细说明。Arduino端代码结构参见README.md
%[text] 你可以将此 Arduino Sketch 保存到另外的位置，以后可以手动在那个位置打开 Arduino IDE，也可以在此处将那个项目主文件ino指定为[winopen](<matlab:doc winopen>)的参数。
%%
%[text] ## 常见问题与解决方法
%[text] 如果代码编译失败，最大的可能原因是C++标准设置错误，可以尝试修复
  %[control:button:4296]{"position":[1,2]}
Gbec.ArduinoCppStandard;
%[text] 修复后必须重启 Arduino IDE。
%[text] 有时IDE会提示更新 Arduino AVR Boards，更新后C++标准会重置为默认的C++11。必须再次运行本节，设为C++17。
%[text] 其它常见编译失败的可能原因包括：
%[text] - 计时器问题，使用了不支持的计时器，例如Uno板只支持0、1、2计时器，Mega才支持3~5
%[text] - 引脚中断问题，使用了不支持中断的引脚。例如电容的OUT引脚必须支持中断，只有`2, 3, 18, 19, 20, 21引脚支持中断。`
%[text] - `检查 Timers_one_for_all Quick_digital_IO_interrupt Cpp_Standard_Library 三个依赖库是否正确安装在【%USERPROFILE%\Documents\Arduino\libraries】中。` \
%%
%[text] 如果修改了 Arduino UID，或者首次配置，都在MATLAB端也重新生成：
  %[control:button:515f]{"position":[1,2]}
Gbec.GenerateMatlabUIDs; %[output:315813ae]
%[text] UID是Arduino与MATLAB进行串口通信的密码本，因此必须保持一致。
%[text] 如果你的 Arduino Sketch 不在默认的位置，将需要指定 Arduino UID.hpp 文件路径作为此函数的参数。详见[Gbec.GenerateMatlabUIDs文档](<matlab:doc Gbec.GenerateMatlabUIDs>)。
%%
%[text] 如果实验运行中发现输出了英文的UID原名或其它翻译错误，可以编辑翻译文件将其翻译为中文：
edit Gbec.LogTranslate;
%%
%[text] 如果上传了新的Arduino代码，执行时却发现实际未更新，可尝试清理Arduino缓存：
MATLAB.IO.Delete('%LOCALAPPDATA%\arduino');

%[appendix]{"version":"1.0"}
%---
%[metadata:view]
%   data: {"layout":"inline","rightPanelPercent":40}
%---
%[control:button:17ff]
%   data: {"label":"打开 Arduino IDE","run":"Section"}
%---
%[control:button:4296]
%   data: {"label":"修复","run":"Section"}
%---
%[control:button:515f]
%   data: {"label":"运行","run":"Section"}
%---
%[output:604529a8]
%   data: {"dataType":"error","outputData":{"errorType":"runtime","text":"错误使用 <a href=\"matlab:matlab.lang.internal.introspective.errorDocCallback('winopen', 'C:\\Program Files\\MATLAB\\R2026a\\toolbox\\matlab\\winfun\\winopen.m', 43)\" style=\"font-weight:bold\">winopen<\/a> (<a href=\"matlab: opentoline('C:\\Program Files\\MATLAB\\R2026a\\toolbox\\matlab\\winfun\\winopen.m',43,0)\">第 43 行<\/a>)\nThe specified file or folder was not found."}}
%---
%[output:315813ae]
%   data: {"dataType":"error","outputData":{"errorType":"runtime","text":"错误使用 <a href=\"matlab:matlab.lang.internal.introspective.errorDocCallback('fileread', 'C:\\Program Files\\MATLAB\\R2026a\\toolbox\\matlab\\io\\filesystem\\fileread.m', 3)\" style=\"font-weight:bold\">fileread<\/a> (<a href=\"matlab: opentoline('C:\\Program Files\\MATLAB\\R2026a\\toolbox\\matlab\\io\\filesystem\\fileread.m',3,0)\">第 3 行<\/a>)\n位置 1 处的参数无效。 值必须为字符向量或字符串标量。\n\n出错 <a href=\"matlab:matlab.lang.internal.introspective.errorDocCallback('Gbec.GenerateMatlabUIDs', 'C:\\Users\\vhtmf\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Gbec\\GenerateMatlabUIDs.m', 29)\" style=\"font-weight:bold\">Gbec.GenerateMatlabUIDs<\/a> (<a href=\"matlab: opentoline('C:\\Users\\vhtmf\\Documents\\MATLAB\\Generic-Behavioural-Experimental-Control\\+Gbec\\GenerateMatlabUIDs.m',29,0)\">第 29 行<\/a>)\nEnumerations=replace(split(extractBetween(fileread(ArduinoUidPath),\"{\",\"}\"),[\",\",newline]),whitespacePattern,'');"}}
%---
