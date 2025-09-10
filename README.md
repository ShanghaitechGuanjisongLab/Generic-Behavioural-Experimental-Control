通用行为实验控制，MATLAB与Arduino串口连接，提供高度可扩展可配置的模块化一站式服务
# 项目背景
实验室的自动化行为实验控制大多用Arduino开发板实现。Arduino开发板是一种嵌入式计算机，可以通过杜邦线引脚连接到硬件设备模组，实现对小鼠的感官刺激和行为记录。为了对实验过程编程，需要编写代码。Arduino官方提供IDE，可以将C++代码编译上传到开发板执行。此外，MATLAB也提供Arduino开发工具箱，允许使用MATLAB语言对Arduino开发板进行编程。基于这两套技术栈，我们实验室流传下来大量经典的行为实验控制代码。

但是，这整个生态系统都存在根本性的严重问题：
- 从 Arduino IDE 上传的C++代码只能在Arduino中执行，无法将收集到的行为数据传回主机进行处理和存储。主机端必须另外配置数据收集解决方案。
- MATLAB工具箱并不能将MATLAB代码编译成Arduino二进制代码，而是将预编译的服务器上传到Arduino，然后主机端MATLAB通过连接Arduino的USB串口一条一条发送指令，交给Arduino执行。这除了导致只能使用MATLAB提供的指令、无法进行多线程编程以外，指令通过串口的传输也存在延迟，对时间精度要求较高的刺激（例如光遗传激光波形、短暂的水泵给水）无法正确完成。
- 祖传代码大多为了特定的实验临时编写，代码设计相对固定，可扩展、可配置性极差，缺乏良好的设计思路。

为了解决这些问题，必须重新编写一个横跨Arduino与主机MATLAB两端的桥梁，并采用高度模块化的代码结构设计，并确保将时间精度要求高的过程由Arduino端独立决策完成。于是有此项目。

本实验控制系统分为Arduino服务端和MATLAB客户端两个部分。使用本系统的优越性：
- 连接PC端和Arduino只为传简单信号，Arduino端独立决策，比用MATLAB端步步操纵Arduino具有更高的时间精度；
- 高度可自定义性，清晰的模块化组件化搭配，可重用性高；
- 随时暂停、放弃实验，不需要拔插头；
- 实验过程中意外掉线自动重连，不会丢失进度
# 配置环境
安装本工具箱之前需要先安装[Arduino IDE 2.0.0 以上版本](https://github.com/arduino/arduino-ide/releases)，以及 Arduino Library Manager 中的Timers_one_for_all 和 Quick_digital_IO_interrupt 库及其依赖库。

此外，本实验系统还依赖[埃博拉酱的MATLAB扩展](https://ww2.mathworks.cn/matlabcentral/fileexchange/96344-matlab-extension)。正常情况下，此工具箱在安装本工具箱时会被自动安装。如果安装失败，可以在MATLAB附加功能管理器中安装。

工具箱安装成功后，请务必查看快速入门指南（GettingStarted.mlx），执行进一步安装配置。

本项目是行为实验通用控制程序，因此针对不同的实验设计，你需要自行编写一些自定义Arduino代码（在Development_Client.mlx中有入口）。最常用的就是ExperimentDesign.cpp，各种实验参数都需要在这里设置好然后上传到Arduino。对于较大的改动，如硬件、回合、实验方案等的增删，可能还需要修改UID.h。

ExperimentDesign.cpp的具体配置语法，在文件中有详细的注释说明。但在那之前，建议先浏览一遍本自述文件。
# 基本概念
使用本项目之前，你需要理解本项目的概念层级。从顶层到底层依次是：

1. 服务器，通常对应串口——Arduino开发板，一个串口开发板就是一个服务器。一台PC可以连接多个服务器，同时运行不同的实验。
2. 进程，服务器上执行的任务。一个服务器上可以也同时执行多个进程，但如果这些进程同时抢占相同的资源，如引脚、串口、计时器等，可能造成意外情况。
3. 模块，进程内具体执行的操作步骤。一个进程可以包含多个模块，这些模块可以顺序执行也可以同步执行，取决于设计方案。

在一次具体的实验会话中，通常需要PC先与服务器串口建立连接，然后创建进程，然后通过指定ID而向进程中载入特定的会话模块，然后开始执行。执行过程中，除了等待执行完毕，还可以暂停、继续、取消、断线重连；进程也可以向PC发送反馈信息，记录实验中发生的各种事件，以及反向控制PC执行一些不便在Arduino上执行的操作。一个服务器上也可以创建多个进程同时执行，但需要注意资源争用问题。实验结束后，可以令Arduino向PC发送本次执行的模块设计详情信息。
# Arduino C++ 代码
上传到Arduino的C++代码决定了一个服务器的具体行为。
## 模块
已经预定义了大量基础模块，用户通常不需要关心基础模块的实现细节，只需要进行低代码的模块组装。在`ExperimentDesign.cpp`中有详细文档说明和示例。概括地说，你需要先设置Arduino上连接各种具体设备的引脚号，然后组装模块，最后选择其中一些顶级的、可以直接单独载入进程执行的模块进行“公开”并赋予ID。这样，以后可以从PC向Arduino发送这些ID指示要执行哪个模块。
## UID与Arduino-PC通信
系统使用UID作为“密码表”实现Arduino与PC的通信。PC向Arduino发送UID指示要运行哪个会话；Arduino向PC发送UID指示当前运行到哪个回合，等等。所有的步骤、回合、会话都具有各自的UID，需要在定义时指定，并将该UID注册在`UID.hpp`中。PC端也需要同样的UID密码表，但可以在安装后用Gbec.GenerateMatlabUIDs自动生成，无需手动操作。

UID具有命名规范，不遵守命名规范可能造成意外错误。除了一些系统保留的内置UID以外，新增的UID必须命名为`类型_实例`结构。常用的类型包括：
- Module，模块ID。已经预定义了一些模块，有些具有固定的ID，也有些允许你指定自定义的ID。自定义的ID必须前缀Module，记录在`UID.hpp`中。这些ID用于Arduino向PC返回模块设计信息时，供人类识读。
- Event，事件ID。会话执行过程中，Arduino会在特定时间发生时向PC反馈Event类UID，PC（MATLAB）端会将事件和发生的时间记录下来，形成实验记录。事件通过`SerialMessage`模块发送。你也可以自定义事件，注册为Event类UID。
- Trial，回合ID。回合是一种特殊的模块，每种不同的回合设计应当有不同的ID。每当你设计了一种未预设的新回合，需要注册Trial类的UID。例如`Trial_BlueAir`表示一个简单的蓝光喷气偶联刺激回合。一个会话内的回合一般应当具有不同的ID，不可能在同一个会话内出现的回合可以共享相同的ID。每个回合开始时，Arduino会将回合ID发送到PC，PC端记录回合开始时间并生成日志等。回合还是断线重连的基本单位，一个未执行完毕的回合在断线重连后会从头开始重新执行；已经执行完毕的回合则不会再重复。不在回合内的模块则一定会重复执行。回合内不允许嵌套回合。
- Session，会话ID。会话是最顶级的模块，可以直接被载入进程执行。每当你设计了一种未预设的新会话，需要指定Session类的UID。例如`Session_BlueAir`表示一个由多次蓝光喷气偶联回合串联成的会话。每一个会话都必须具有独特的ID，不允许任何两个会话拥有相同的ID，因为PC端需要通过ID唯一指定一个具体的会话载入进程。
- Test，测试ID。测试是一种轻量级的会话，同样可以由PC指定直接载入进程执行，用于实验前检查各种设备是否正常工作。例如`Test_BlueLed`检查蓝色LED是否能点亮。你编写的任何自定义测试也需要注册为Test类UID。
- Host，主机动作ID。如果你需要Arduino反控PC执行一些不便在Arduino上执行的动作（如拍摄视频、屏幕显示图像等），需要由Arduino通过`SerialMessage`模块向PC发送Host类的UID。PC收到该类UID后将执行你预先定义的动作。
## 高级
如果上述配置无论怎样组合都无法满足你的需求，你将需要一些更复杂的编码工作。本文假定你对C++语言熟练掌握或具有足够强的检索能力，不再对一些术语进行解释。下面列出一些常见高级更改的指南。

如果预定义的基础模块都不符合需求，或者你的步骤具有特殊的复杂控制方式，你将需要自己写控制代码来控制该设备。所有步骤必须实现Module接口：
```C++
// 所有模块的基类，本身可以当作一个什么都不做的空模块使用
struct Module {
	Process &Container;
	Module(Process &Container)
	  : Container(Container) {
	}
	// 返回是否需要等待回调，并提供回调函数。返回true表示模块还在执行中，将在执行完毕后调用回调函数；返回false表示模块已执行完毕，不会调用回调函数。
	virtual bool Start(std::move_only_function<void()> &FinishCallback) {}
  // PC端要求每个模块写出信息时执行此方法。模块应当向流写出自己的信息。
	virtual void WriteInfo(std::ostringstream &InfoStream) const {};
	// 放弃该模块。未在执行的模块放弃也不会出错。
	virtual void Abort() {}
	// 重新开始当前执行中的模块。
	virtual void Restart() {}
	virtual ~Module() = default;
  //模块内包含的回合数。如果你的模块可以包含确定数目的回合，将此值设为非0。
	static constexpr uint16_t NumTrials = 0;
};
```
每个模块都可以访问`Processs`容器`Container`，并调用进程容器提供的服务：
```C++
struct Process
{
  //注册一个引脚中断。当指定引脚RISING时，触发Callback回调。返回一个指针供UnregisterInterrupt时使用
  PinListener *RegisterInterrupt(uint8_t Pin, std::move_only_function<void()> &&Callback);

  //取消中断
	void UnregisterInterrupt(PinListener *Handle);

  //分配一个空闲的计时器，支持各种定时操作。计时器的用法参考Timers_one_for_all的文档。
	Timers_one_for_all::TimerClass *AllocateTimer();

	//释放计时器。此操作不负责停止计时器，仅使其可以被再分配
	void UnregisterTimer(Timers_one_for_all::TimerClass *Timer);

  //载入模块。如果你的模块引用其它模块，需要用此方法载入并获取其指针，而不是自行new模块。容器进程可以确保相同模块不会被重复创建。
	template<typename ModuleType>
	ModuleType *LoadModule();
}
```
要向串口发送复杂的信息，使用`SerialStream`全局对象而不是Arduino自带的`Serial`对象，使用方法详见`Async_stream_IO.hpp`中的说明文本：
```C++
extern Async_stream_IO::AsyncStream *SerialStream;
```
可以参见`Predefined.h`中的各种基础模块的示例写法。在进行高级开发时，你还需要了解以下UID类型：
- Field，信息字段ID。在你设计的模块中WriteInfo时，需要向InfoStream写入Field类型的UID以标识不通的信息字段供人类识读。
- Column，表列ID。此类ID仅针对表格类型的信息，标识列名。
- Type，数据类型ID，用于提示PC端应当以何种类型识别串口字节。
# MATLAB代码结构
使用前需导入包：
```MATLAB
import Gbec.*
```
安装阶段生成的Development_Client, Experiment_Client和SelfCheck_Client是你的控制面板。一般先根据脚本中已有的提示信息进行Arduino端配置、设备检查（SelfCheck_Client），然后开始实验（Experiment_Client）

每个类/函数的详细文档可用doc命令查看。以下列出简要概述：

类
```MATLAB
classdef Formal<Gbec.Process
	%正规实验进程，提供完善可靠的数据收集系统和自定义接口
end
classdef GratingImage<Gbec.IHostAction
	%示例类，用于在HostAction中显示栅格图
end
classdef IHostAction<handle
	%设为HostAction所必须实现的接口。
end
classdef Process<handle
	%进程基类。通常不应直接创建，而是使用其派生类Test和Formal。
end
classdef Server<handle
	%控制服务器串口连接
end
classdef Test<Gbec.Process
	%轻量级测试类进程，扩展一些便于手动测试设备的实用方法
end
classdef VideoRecord<Gbec.IHostAction
	%视频录制主机动作
end
```
函数
```MATLAB
%本函数尝试修复 Arduino C++ 标准配置错误的问题。Arduino Mega AVR 开发板默认使用C++11标准编译，但本工具箱的Arduino代码使用了C++17新特性，因此必须改用C++17标准编译。推荐使用，Due SAM 开发板，内存更充裕且不会遇到此问题。
function ArduinoCppStandard

%根据Arduino端的UIDs.hpp，生成MATLAB端的UIDs.m
function GenerateMatlabUIDs

%安装通用行为控制工具箱的向导
function Setup

%该函数用于将输出的英文日志翻译成中文。如果你发现实验中输出的日志信息含有英文，而你希望将它翻译成方便阅读的中文，可以模仿已有条目增加新的翻译项。也可以对已有项进行删改。
function LogTranslate
```
脚本
```MATLAB
%此脚本用于Arduino端相关的配置，如代码上传、错误排除等
Development_Client

%实验主控台脚本。在这里完成相关配置，并开始实验。通常你需要设定好串口号，选择要运行的会话，设置保存路径，是否自动关闭串口，设置喵提醒码（如果要用），视频拍摄设置。实验进程中，如需暂停、放弃、继续、返回信息、断开串口等操作，则运行脚本中相应节。
Experiment_Client

%实验开始前，应当检查各项设备是否运行正常。该脚本中提供了多种常用的设备检查命令。你也可以照例增添新设备的检查命令。
SelfCheck_Client
```