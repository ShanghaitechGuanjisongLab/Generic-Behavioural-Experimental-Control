classdef IHostAction<handle
	%设为HostAction所必须实现的接口。
	%用户需要自行定义一个类，继承自IHostAction，并实现所有抽象方法。这样可以在实验运行中在主机上执行任何自定义动作。
	%Gbec.GratingImage是一个示例类，展示IHostAction的一种实现。
	%See also Gbec.GratingImage
	methods(Abstract)
		%Arduino发来信号时执行此方法，在这里实现要进行的主机动作代码
		Run(obj)

		%实验结束后执行此方法，返回实验要保存的信息
		Info=GetInformation(obj)

		%实验提前中断时调用，应停止任何正在执行的动作并执行清理
		Abort(obj)
	end
end