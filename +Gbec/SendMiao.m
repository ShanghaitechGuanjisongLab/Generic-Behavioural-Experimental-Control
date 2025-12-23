%[text] 发送喵提醒
%[text] ## 语法
%[text] ```matlabCodeExample
%[text] Gbec.SendMiao(MiaoCode);
%[text] %向指定喵码发送提醒
%[text] 
%[text] Gbec.SendMiao(MiaoCode,Note);
%[text] %额外指定提醒文本
%[text] 
%[text] Gbec.SendMiao(___,HttpRetryTimes);
%[text] %与上述任意语法组合使用，额外指定HTTP重试次数
%[text] ```
%[text] ## 输入参数
%[text] MiaoCode(1,:)char，喵码
%[text] Note(1,:)char，提醒文本
%[text] HttpRetryTimes(1,1)=0，HTTP重试次数
function SendMiao(MiaoCode,varargin)
Note='';
HttpRetryTimes=0;
for V=1:numel(varargin)
	if isnumeric(varargin{V})
		HttpRetryTimes=varargin{V};
	else
		Note=varargin{V};
	end
end
if~isempty(Note)
	Note="&text="+Note;
end
Note="http://miaotixing.com/trigger?id="+MiaoCode+Note;
for a=0:HttpRetryTimes
	try
		%HttpRequest不支持中文，必须用webread
		webread(Note);
	catch ME
		if strcmp(ME.identifier,'MATLAB:webservices:UnknownHost')
			continue;
		else
			warning(ME.identifier,'%s',ME.message);
			break;
		end
	end
	break;
end
end

%[appendix]{"version":"1.0"}
%---
