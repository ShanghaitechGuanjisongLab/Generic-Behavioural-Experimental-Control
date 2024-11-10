function Setup_Impl
import MATLAB.IO.CopyFile
import MATLAB.IO.Delete
% 不能 import System.IO.*，如果用户未安装.NET桌面运行时，将不会产生正确的错误输出
WorkingDirectory=uigetdir("","选择工作目录");
UserDirectory=fullfile(userpath,'+Gbec');
DeployDirectory=fullfile(fileparts(fileparts(fileparts(mfilename("fullpath")))),'部署');
DeployToUser=fullfile(DeployDirectory,'+Gbec');
DeployToWorking=fullfile(DeployDirectory,'工作目录');
VerFile=fullfile(UserDirectory,'.ver');
if isfolder(UserDirectory)
	ArduinoDirectory=fullfile(UserDirectory,'Gbec');
	if isfile(VerFile)
		Fid=fopen(VerFile);
		RequestOverride=fread(Fid,1,'uint8=>uint8')<Gbec.Version().Deploy;
		fclose(Fid);
		RequestOverride=isempty(RequestOverride)||RequestOverride;
	else
		RequestOverride=true;
	end
	if RequestOverride
		if questdlg('无法保留以前版本的用户配置','是否覆盖？','确定','取消','确定')=="确定"
			try
				Delete(ArduinoDirectory);
			catch ME
				if ME.identifier~="MATLAB:MatlabException:File_operation_failed"
					ME.rethrow;
				end
			end
			OverwriteInstall(DeployToUser,DeployToWorking,WorkingDirectory,VerFile);
		else
			disp('已放弃安装');
			return
		end
	else
		OldPaths=string(System.IO.Directory.GetFiles(ArduinoDirectory,'*',System.IO.SearchOption.AllDirectories));
		[~,OldNames]=fileparts(OldPaths);
		[~,NewNames]=fileparts(string(System.IO.Directory.GetFiles(fullfile(DeployToUser,'Gbec'),'*',System.IO.SearchOption.AllDirectories)));
		[~,Index]=setdiff(OldNames,NewNames);
		Delete(OldPaths(Index));
		[FromUser,ToUser]=FilterPaths(UserDirectory,DeployToUser);
		[FromWorking,ToWorking]=FilterPaths(WorkingDirectory,DeployToWorking);
		CopyFile([FromUser,FromWorking],[ToUser,ToWorking]);
	end
elseif isfolder(WorkingDirectory)
	if questdlg('无法保留以前版本的用户配置','是否覆盖？','确定','取消','确定')=="确定"
		OverwriteInstall(DeployToUser,DeployToWorking,WorkingDirectory,VerFile);
	else
		disp('已放弃安装');
		return
	end
else
	OverwriteInstall(DeployToUser,DeployToWorking,WorkingDirectory,VerFile);
end
cd(WorkingDirectory);
try
	Gbec.ArduinoCppStandard;
catch ME
	if ME.identifier~="MATLAB:fopen:InvalidInput"
		ME.rethrow;
	end
end
edit Development_Client
end
function [FromPaths,ToPaths]=FilterPaths(OldDirectory,NewDirectory)
persistent KeepNames
if isempty(KeepNames)
	KeepNames=["UID.h","UID.m","ExperimentDesign.h","Experiment_Client.mlx","SelfCheck_Client.mlx","LogTranslate.mlx"];
end
[~,OldNames,OldExtensions]=fileparts(string(System.IO.Directory.GetFiles(OldDirectory,'*',System.IO.SearchOption.AllDirectories)));
NewPaths=string(System.IO.Directory.GetFiles(NewDirectory,'*',System.IO.SearchOption.AllDirectories));
[~,NewNames,NewExtensions]=fileparts(NewPaths);
[~,Index]=setdiff(NewNames+NewExtensions,intersect(KeepNames,OldNames+OldExtensions));
FromPaths=NewPaths(Index);
ToPaths=arrayfun(@(FP)fullfile(OldDirectory,string(System.IO.Path.GetRelativePath(NewDirectory,FP))),FromPaths);
end
function OverwriteInstall(DeployToUser,DeployToWorking,WorkingDirectory,VerFile)
if MATLAB.IO.CopyFile(DeployToUser,userpath)||MATLAB.IO.CopyFile(fullfile(DeployToWorking,'*'),WorkingDirectory)
	Gbec.Exception.User_canceled_operation.Throw;
end
Gbec.GenerateMatlabUIDs;
Fid=fopen(VerFile,'w');
fwrite(Fid,Gbec.Version().Deploy,'uint8');
fclose(Fid);
end