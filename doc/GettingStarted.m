%[text] 首次使用需要安装，将生成3个重要的工作脚本：
%[text] - Experiment\_Client，用于设置实验各项参数
%[text] - SelfCheck\_Client，用于实验前检查设备工作状况
%[text] - Development\_Client，用于修改Arduino端 \
%[text] 运行之前请确认：
%[text] - 安装了 Arduino IDE 2.0.0 以上版本
%[text] - 在IDE中安装了开发板 Arduino SAM Boards （Due开发板，推荐）或 Arduino AVR Boards （Mega开发板，不推荐，内存偏小），和库Timers\_one\_for\_all（v3.0.0+）、Quick\_digital\_IO\_interrupt（v3.0.0+）及它们的所有依赖库 \
%[text] 如果 Arduino IDE 库管理器中未列出Timers\_one\_for\_all和Quick\_digital\_IO\_interrupt，可选手动安装（应优先用 Arduino IDE 安装，不行再手动：
if questdlg('在 Arduino IDE 中尝试安装了Timers_one_for_all（v3.0.0+）和Quick_digital_IO_interrupt（v3.0.0+）吗？','请确认','成功了','失败了','还没试','成功了')=="失败了"
	InstallLibrary('Timers_one_for_all', '3.0.0');
	InstallLibrary('Quick_digital_IO_interrupt', '3.0.0');
end
%%
%[text] 运行以下代码，选择一个工作目录用于存放这三个脚本。如果是升级安装，我们会尽可能为您保留原有的工作脚本和Arduino实验设计代码等用户配置文件。如果无法保留，将会提示确认；如果希望全新安装，请先执行下一节的清理再安装。
Gbec.Setup; %[output:21bce9c1]
%[text] 一般来说，应先打开Development\_Client，配置Arduino端，然后SelfCheck\_Client检查设备，最后Experiment\_Client运行实验。
%%
%[text] 卸载本工具箱之前请执行清理
MATLAB.IO.Delete(fullfile(userpath,'+Gbec'));
%%
function InstallLibrary(Name,Version)
persistent libsDir
if isempty(libsDir)
	libsDir = fullfile(getenv('USERPROFILE'), 'Documents\Arduino\libraries');
end
warning off MATLAB:MKDIR:DirectoryExists;
mkdir(libsDir);
NameVersion = sprintf('%s-%s', Name, Version);
zipPath = fullfile(tempdir,NameVersion+".zip");
if ~isfile(zipPath)
	websave(zipPath, sprintf('https://github.com/Ebola-Chan-bot/%s/archive/refs/tags/v%s.zip', Name, Version));
end
unzip(zipPath, libsDir);
UnzipPath = fullfile(libsDir,NameVersion);
TargetPath=fullfile(libsDir,Name);
try
	MATLAB.IO.MoveFile(UnzipPath, TargetPath);
catch ME
	if ME.identifier=="MATLAB:Exception:File_operation_failed"
		MATLAB.IO.Delete(UnzipPath);
	else
		ME.rethrow;
	end
end
end

%[appendix]{"version":"1.0"}
%---
%[metadata:view]
%   data: {"layout":"inline","rightPanelPercent":40}
%---
%[output:21bce9c1]
%   data: {"dataType":"text","outputData":{"text":"已放弃安装\n","truncated":false}}
%---
