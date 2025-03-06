function SendMiao(Note,HttpRetryTimes,MiaoCode)
for a=0:HttpRetryTimes
	try
		%HttpRequest不支持中文，必须用webread
		webread(sprintf('http://miaotixing.com/trigger?id=%s&text=%s',MiaoCode,Note));
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