classdef VideoRecord<Gbec.IHostAction
	properties
		VideoInput
	end
	methods
		function obj = VideoRecord(VideoInput)
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