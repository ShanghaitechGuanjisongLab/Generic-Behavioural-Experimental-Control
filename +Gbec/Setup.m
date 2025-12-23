function Setup
%  SETUP 安装通用行为控制工具箱的向导
% 
% 该向导将引导用户选择一个工作目录，存放Experiment_Client和SelfCheck_Client两个客户端界面脚本，以及选择Arduino端的UIDs.h以生成MATLAB端的对应文件
feature("USE_DOTNETCLI2_WINDOWS", 1);%绕过.NET9导致的崩溃问题
try
	dotnetenv('core');
catch ME
	if strcmp(ME.identifier,'MATLAB:netenv:NETLoaded')
		Gbec.Exception.Unsupported_NET_environment_please_restart_MATLAB.Throw;
	else
		ME.rethrow;
	end
end
%R2023a以前，调用.NET的代码不能和dotnetenv放在同一个代码文件中，会导致.NET环境提前载入，无法切换到core，因此将Setup的实现单独作为私有函数。R2023a已修复此问题，但已写好的代码没有必要再修改。
try
	Setup_Impl;
catch ME
	if ME.identifier=="MATLAB:NET:InvalidCoreVersion"
		error(ME.identifier,strcat(ME.message,'\n请<a href="https://dotnet.microsoft.com/zh-cn/download/dotnet">下载最新版.NET桌面运行时 Windows x64</a>，然后重启MATLAB'));
	else
		ME.rethrow;
	end
end
end