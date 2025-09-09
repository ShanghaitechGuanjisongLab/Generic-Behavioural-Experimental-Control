classdef VideoRecord<Gbec.IHostAction
	properties
		%视频硬件适配器对象
		VideoInput
	end
	methods
		function obj = VideoRecord(VideoInput)
			%# 语法
			% ```
			% obj=Gbec.VideoRecord(VideoInput)
			% ```
			%# 输入参数
			% VideoInput(1,1)videoinput，视频硬件适配器对象，可用内置videoinput方法获取
			%See also videoinput
			obj.VideoInput = VideoInput;
		end
		function Run(obj)
			if obj.VideoInput.Logging=="off"
				trigger(obj.VideoInput);
			end
		end
		function Info=GetInformation(obj)
			Info=struct;
			Info.Name=obj.VideoInput.Name;
			if~isempty(obj.VideoInput.DiskLogger)
				Info.Filename=obj.VideoInput.DiskLogger.Filename;
			end
			Info.TriggerRepeat=obj.VideoInput.TriggerRepeat;
			Info.FramesPerTrigger=obj.VideoInput.FramesPerTrigger;
			Info.LoggingMode=obj.VideoInput.LoggingMode;
			Info.TriggerType=obj.VideoInput.TriggerType;
		end
		function Abort(obj)
			if obj.VideoInput.Logging=="on"
				stop(obj.VideoInput);
			end
		end
	end
end