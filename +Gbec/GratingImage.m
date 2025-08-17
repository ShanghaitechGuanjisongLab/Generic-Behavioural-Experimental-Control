classdef GratingImage<Gbec.IHostAction
	%示例类，用于在HostAction中显示栅格图
	%栅格图的各项参数属性，如无特殊说明，均可以三种方式指定：
	% - 标量，表示该参数总是该值
	% - (:,1)，表示该参数每次在列向量中随机抽一个元素作为取值
	% - (1,2)，表示该参数以行向量中的两个元素为上下界，在此范围内随机抽一个取值
	%此类不支持直接构造，请用静态方法New获取实例
	properties(SetAccess=immutable,GetAccess=protected)
		Window
		Timer timer
		Width single
		Height single
		Logger MATLAB.DataTypes.EventLogger
	end
	properties(Access=protected)
		CurrentImage
		CircularFrequency
		Reciprocal
	end
	properties(Dependent)
		%每个周期的像素数
		PixelsPerCycle
		%图像宽度是周期的几倍
		CyclesPerWidth
		%图像高度是周期的几倍
		CyclesPerHeight
	end
	properties
		%旋转角度
		AngleRange
		%呈现秒数
		DurationRange
		%偏移相位
		InitialPhase
		%颜色渐变范围
		%(:,3)uint8矩阵，栅格色谱，作为插值锚点
		ColorRange
	end
	methods
		function obj = GratingImage(options)
			%# 语法
			% ```
			% obj=Gbec.GratingImage.New(Name=Value);
			% ```
			%# 名称值参数
			% 使用名称值参数指定栅格图的各项参数属性，如无特殊说明，均可以三种方式指定：
			%  - 标量，表示该参数总是该值
			%  - (:,1)，表示该参数每次在列向量中随机抽一个元素作为取值
			%  - (1,2)，表示该参数以行向量中的两个元素为上下界，在此范围内随机抽一个取值
			% 以下三个参数，如不指定将取默认值：
			%  - AngleRange=[-pi/2,pi/2]，顺时针旋转弧度。0表示在水平方向上周期变化，竖直方向上不变的栅格图。
			%  - DurationRange=Inf，图像呈现的秒数。
			%  - InitialPhase=0，周期变化的初始正弦相位弧度，例如pi/2表示从峰值开始
			%  - ColorRange(:,3)uint8=[0,0,0;255,255,255]，周期性渐变的颜色梯度，第1维是不同的颜色，第2维RGB。首行是正弦周期的谷值点的颜色，末行是峰值点颜色，中间其'
			%   它颜色线性插值。
			% 以下三个参数必须指定其一，不可多不可少：
			%  - PixelsPerCycle，每个周期的像素数
			%  - CyclesPerWidth，图像宽度是周期的几倍
			%  - CyclesPerHeight，图像高度是周期的几倍
			arguments
				options.AngleRange=[-pi/2,pi/2]
				options.DurationRange=Inf
				options.InitialPhase=0
				options.PixelsPerCycle
				options.CyclesPerWidth
				options.CyclesPerHeight
				options.ColorRange uint8=[0,0,0;255,255,255]
			end
			ScreenTable=MATLAB.Graphics.Window.Screens;
			FirstSecondary=find(~ScreenTable.IsPrimary,1);
			if isempty(FirstSecondary)
				FirstSecondary=1;
			end
			obj.Window=MATLAB.Graphics.Window.Create(DeviceName=ScreenTable.DeviceName(FirstSecondary));
			Rectangle=ScreenTable.Rectangle(FirstSecondary,:);
			obj.Window.Fill([0xff,0,0,0]);
			obj.Width=Rectangle(3)-Rectangle(1);
			obj.Height=Rectangle(4)-Rectangle(2);
			WeakReference=matlab.lang.WeakReference(obj);
			obj.Timer=timer(TimerFcn=@(~,~)WeakReference.Handle.Window.RemoveVisual(WeakReference.Handle.CurrentImage));
			obj.AngleRange=options.AngleRange;
			obj.DurationRange=options.DurationRange;
			obj.InitialPhase=options.InitialPhase;
			obj.ColorRange=options.ColorRange;
			Fields=["PixelsPerCycle","CyclesPerWidth","CyclesPerHeight"];
			Field=Fields(isfield(options,Fields));
			if isscalar(Field)
				obj.(Field)=options.(Field);
			else
				Gbec.Exceptions.Grating_properties_invalid.Throw(sprintf('必须指定 %s 之一',join(Fields,' ')));
			end
			obj.Logger=MATLAB.DataTypes.EventLogger;
		end
		function PC=get.PixelsPerCycle(obj)
			if obj.Reciprocal
				PC=2*pi*obj.CircularFrequency;
			else
				PC=2*pi./obj.CircularFrequency;
			end
		end
		function set.PixelsPerCycle(obj,PC)
			obj.Reciprocal=isequal(size(PC),1:2);
			if obj.Reciprocal
				obj.CircularFrequency=PC/(2*pi);
			else
				obj.CircularFrequency=(2*pi)/PC;
			end
		end
		function CW=get.CyclesPerWidth(obj)
			CW=obj.Width/obj.PixelsPerCycle;
		end
		function set.CyclesPerWidth(obj,CW)
			obj.Reciprocal=false;
			obj.CircularFrequency=sort((2*pi*CW)/obj.Width);
		end
		function CH=get.CyclesPerHeight(obj)
			CH=obj.Height/obj.PixelsPerCycle;
		end
		function set.CyclesPerHeight(obj,CH)
			obj.Reciprocal=false;
			obj.CircularFrequency=sort((2*pi*CH)/obj.Height);
		end
		function Info=Test(obj)
			CF=GetValue(obj.CircularFrequency);
			if obj.Reciprocal
				CF=1/CF;
			end
			Angle=GetValue(obj.AngleRange);
			Duration=GetValue(obj.DurationRange);
			IP=GetValue(obj.InitialPhase);
			Image=permute(uint8(interp1(linspace(-1,1,height(obj.ColorRange)),single(obj.ColorRange),sin(((1:obj.Width)'*cos(Angle)+(1:obj.Height)*sin(Angle))*CF+IP))),[3,1,2]);
			Image(4,:,:)=255;
			obj.CurrentImage=obj.Window.Image(Image);
			if ~isinf(Duration)
				obj.Timer.StartDelay=Duration;
				obj.Timer.start;
			end
			Info=struct(CircularFrequency=CF,Angle=Angle,Duration=Duration,InitialPhase=IP,Image=Image);
		end
		function Run(obj)
			%Arduino发来HostAction信号时，执行此方法
			obj.Logger.LogEvent(obj.Test);
		end
		function Information=GetInformation(obj)
			%会话结束后，通过此方法获取信息
			Events=obj.Logger.GetTimeTable;
			Information=struct(Width=obj.Width,Height=obj.Height,PixelsPerCycle=obj.PixelsPerCycle,AngleRange=obj.AngleRange,DurationRange=obj.DurationRange,InitialPhase=obj.InitialPhase,ColorRange=obj.ColorRange);
            if ~isempty(Events)
                Information.Events=[timetable(Events.Time),struct2table(Events.Event,AsArray=true)];
            end
		end
		function Abort(obj)
			%实验提前中断时调用，应停止任何正在执行的动作并执行清理
			if obj.Timer.Running=="on"
				obj.Timer.stop;
			end
			if ~isempty(obj.CurrentImage)
				obj.Window.RemoveVisual(obj.CurrentImage);
			end
		end
		function delete(obj)
			delete(obj.Timer);
			delete(obj.Window);
		end
	end
end
function Value=GetValue(Value)
if isequal(size(Value),1:2)
	[Min,Max]=bounds(Value);
	Value=rand*(Max-Min)+Min;
elseif ~isscalar(Value)
	Value=randsample(Value,1);
end
end