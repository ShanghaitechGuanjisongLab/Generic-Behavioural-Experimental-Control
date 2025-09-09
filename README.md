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
# Arduino C++ 代码结构
使用本项目之前，你需要理解本项目的概念层级。

从顶级到低级，一次实验包含会话、回合、步骤3个层级。我们描述这些层级的常用说法包括：

- 会话：今天做了一个蓝光喷气闭眼实验，喷气100次，随机穿插了20个不喷气的Trial
- 回合：这个Trial没给喷气刺激
- 步骤：实验一开始是一个Pretrial等待阶段，需要连续6秒不舔水才继续

因此我们得到3个概念的定义：
- 步骤：定义了一段具有特殊性质的时间控制流程。一些预定义的模板步骤可在ExperimentDesign.h中查看
- 回合：定义了几个时相的顺序组合，并可以在会话当中以固定或随机的顺序重复交替运行。
- 一次实验的最顶级控制单元。一个会话可以包含多个不同的回合，并且可以设置这些回合各自的重复次数，以及这些回合是按固定顺序还是随机顺序交替运行。
## UID与Arduino-PC通信
系统使用UID作为“密码表”实现Arduino与PC的通信。PC向Arduino发送UID指示要运行哪个会话；Arduino向PC发送UID指示当前运行到哪个回合，等等。所有的步骤、回合、会话都具有各自的UID，需要在定义时指定，并将该UID注册在UID.h中。PC端也需要同样的UID密码表，但可以在安装后用Gbec.GenerateMatlabUIDs自动生成，无需手动操作。

UID具有命名规范，不遵守命名规范可能造成意外错误。除了一些系统保留的内置UID以外，新增的UID必须命名为`类型_实例`结构。常用的类型包括：
- Step。如果你设计了一种未列出的新步骤，需要指定Step类的UID。例如`Step_Calmdown`表示一个冷静步骤。一般来说，只要保证同一个回合内的时相具有不同的UID即可，不会出现在同一个回合内的时相可以共享相同的UID。
- Trial。每当你设计了一种未预设的新回合，需要指定Trial类的UID。例如`Trial_BlueAir`表示一个简单的蓝光喷气偶联刺激回合。一个会话内的回合一般应当具有不同的UID，不可能在同一个会话内出现的回合可以共享相同的UID。
- Session。每当你设计了一种未预设的新会话，需要指定Session类的UID。例如`Session_BlueAir`表示一个由多次蓝光喷气偶联回合串联成的会话。每一个会话都必须具有独特的UID，不允许任何两个会话拥有相同的UID，因为PC端需要通过UID唯一指定一个具体的会话要求Arduino运行。

还有一些其它类型的UID用法，参见[高级](#高级)部分。
## 测试
在实验开始之前通常要进行设备测试。在ExperimentDesign.h中提供了一些预定义的测试，包括引脚闪烁测试和监视器测试，你也可以自行添加。在SelfCheck_Client.mlx中可以运行测试。测试时，PC端将发送UID来指示Arduino要测试哪个设备，因此参与测试的所有设备必须具有各不相同的UID。因为一种设备可以对应多个设备对象，只要这些对象具有不同的UID，你就可以用多种方法对设备进行测试。
## 计时器设置
对于具有时间控制的对象，必须指定一个物理计时器。Arduino Mega 2560开发板具有0、1、2、3、4、5共6个计时器。其中0精度最低，1、3、4、5精度最高，2居中。一个计时器只能同时执行一个计时任务，即凡是有可能同时运行的两个对象，都不能指定同一个计时器。ExperimentDesign.h中提供了预定义的优化计时器分配方案，如果你有特殊需求（比如同时给两个CS），可能需要自定义分配方案。
## 高级
如果上述配置无论怎样组合都无法满足你的需求，你将需要一些更复杂的编码工作。本文假定你对C++语言熟练掌握或具有足够强的检索能力，不再对一些术语进行解释。下面列出一些常见高级更改的指南。

如果项目中预设的步骤模板都不符合需求，或者你的步骤具有特殊的复杂控制方式，你将需要自己写控制代码来控制该设备。所有步骤必须实现IStep接口：
```C++
struct IStep {
  //会话开始前将被所在回合调用一次，通常用于设置pinMode等初始化状态。
  virtual void Setup() const {}
  /*
  步骤开始时，此方法被所在回合调用。步骤可分为立即完成、无需等待直接进入下一步骤的即时步骤，和需要等待一段时间才能完成的延时步骤。
  对于即时步骤，可以直接忽略FinishCallback参数，执行完步骤内容后返回false即可，控制权回到回合后将立即进入下一步骤。
  对于延时步骤，应当保存FinishCallback，然后返回true，提示回合必须等待该步骤调用FinishCallback。步骤应当设置一个时间和/或条件中断，在中断处理函数中调用FinishCallback表示步骤结束，控制权回到回合。
  参考Predefined.h中预先定义的步骤：
  PinFlashStep就是一个典型的即时步骤，它将引脚设置为高电平，然后用一个计时器中断在一定时间后将其设为低电平；但是步骤本身不占用主轴时间，立即返回true，进入下一步。
  WaitStep则是一个简单的延时步骤，它设置一个计时器中断，在一定时间后调用FinishCallback，本身则返回false，提示回合应当等待。而不进入下一步。
  */
  virtual bool Start(void (*FinishCallback)()) const {};
  //  此方法并非必须实现。对于即时步骤，因为步骤立即完成不会被暂停，不需要实现此方法。对于延时步骤，一般应当实现此方法，否则暂停指令将被忽略。
  virtual void Pause() const {}
  //如果你实现了Pause，那么显然也必须有一个对应的Continue实现。
  virtual void Continue() const {}
  // 默认和暂停相同，如果没有特殊的Abort实现就不需要重写
  virtual void Abort() const {
    Pause();
  }
  static constexpr UID Info = Step_Null;
};
```
同样地，你可以编写自己的测试，需要实现ITest接口：
```C++
struct ITest {
  UID MyUID;
  // 测试开始时将调用此方法。测试分为自动结束型和手动结束型。对于自动结束型，应当根据TestTimes参数将测试重复指定的次数，并返回true表示该测试将自动结束。对于手动结束型，一般应当忽略TestTimes参数，返回false，持续测试直到Stop被调用。
  virtual bool Start(uint16_t TestTimes) const = 0;
  //测试被用户手动结束时将调用此方法。自动结束型测试无需实现此方法，手动结束型则必须实现。
  virtual void Stop() const {}
  constexpr ITest(UID MyUID)
    : MyUID(MyUID) {}
};
```
可以参见Predefined.h中的的示例写法。
# MATLAB代码结构
使用前需导入包：
```MATLAB
import Gbec.*
```
安装阶段生成的Development_Client, Experiment_Client和SelfCheck_Client是你的控制面板。一般先根据脚本中已有的提示信息进行Arduino端配置、设备检查（SelfCheck_Client），然后开始实验（Experiment_Client）

每个类/函数的详细文档可用doc命令查看。以下列出公开接口：

类
```MATLAB
classdef ExperimentWorker<handle
    %实验主控制器
    %一般不直接使用该类，而是通过*_Client.mlx实时脚本作为用户界面来操纵实验。当然您也可以根据我们提供的Client脚本学习本类的使用方法。
end
classdef UIDs<uint8
	%该枚举类中记录了和Arduino端完全一致的UID密码表，供MATLAB端参考，识别从串口发来的字节编码。如果要修改UID密码表，应当在Arduino端修改UIDs.h，然后运行GenerateMatlabUIDs.mlx，自动生成UIDs.m，不要手动修改该文件。
end
```
函数
```MATLAB
%本函数尝试修复 Arduino C++ 标准配置错误的问题。Arduino默认使用C++11标准编译，但本工具箱的Arduino代码使用了C++17新特性，因此必须改用C++17标准编译。
function ArduinoCppStandard
%根据Arduino端的UIDs.h，生成MATLAB端的UIDs.m
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