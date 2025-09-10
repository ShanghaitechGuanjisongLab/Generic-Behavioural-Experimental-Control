%[text] 根据Arduino端的UIDs.hpp，生成MATLAB端的UIDs.m
%[text] ## 语法
%[text] ```matlabCodeExample
%[text] Gbec.GenerateMatlabUIDs;
%[text] %在默认位置查找UID.hpp
%[text] 
%[text] Gbec.GenerateMatlabUIDs(ArduinoUidPath);
%[text] %在指定路径查找UID.hpp
%[text] ```
%[text] ## 输入参数
%[text] ArduinoUidPath(1,1)string，指定 Arduino UID.hpp 路径，默认%USERPROFILE%\\Documents\\MATLAB\\+Gbec\\Gbec\\UID.hpp。如果在路径上找不到文件，将弹出文件选择对话框要求用户手动指定UID文件位置。**这个对话框可能会藏在MATLAB主窗口下面并且在任务栏中没有显示图标，此时MATLAB不会响应大多数命令，这并非“卡死”而是模态对话框在等待你交互，请注意查找。**
function GenerateMatlabUIDs(ArduinoUidPath)
persistent Prefix Suffix MatlabUidPath UserDirectory
if isempty(Prefix)
	Prefix="classdef UID<uint8&MATLAB.Lang.IEnumerableException"+newline+"	%此枚举类由`GenerateMatlabUIDs`根据Arduino端UID.hpp自动生成，通常不应手动修改。"+newline+"	enumeration";
	Suffix=newline+"	end"+newline+"end";
	UserDirectory=fullfile(userpath,'+Gbec');
	MatlabUidPath=fullfile(UserDirectory,'UID.m');
end
if ~nargin
	ArduinoUidPath=fullfile(UserDirectory,'Gbec\UID.hpp');
end
if ~isfile(ArduinoUidPath)
	ArduinoUidPath=MATLAB.UITools.OpenFileDialog(Filter='Arduino UID|UID.hpp',Title='选择 Arduino UID.hpp 头文件');
end
Enumerations=replace(split(extractBetween(fileread(ArduinoUidPath),"{","}"),[",",newline]),whitespacePattern,'');
Enumerations=Enumerations(contains(Enumerations,textBoundary('start')+lettersPattern));
HasDefinition=contains(Enumerations,'=');
DefinedEnumerations=Enumerations(HasDefinition);
AutoEnumerations=Enumerations(~HasDefinition);
if isempty(DefinedEnumerations)
	AutoStart=-1;
else
	DefinedEnumerations=split(DefinedEnumerations,'=');
	AutoStart=eval(string(DefinedEnumerations(end,2)));
	DefinedEnumerations=DefinedEnumerations(:,1)+"("+DefinedEnumerations(:,2)+")";
end
AutoEnumerations=AutoEnumerations+"("+string(AutoStart+1:AutoStart+numel(AutoEnumerations))'+")";
Fid=fopen(MatlabUidPath,"w");
fwrite(Fid,Prefix+join(newline+"		"+[DefinedEnumerations;AutoEnumerations],'')+Suffix,"char");
fclose(Fid);

%[appendix]{"version":"1.0"}
%---
