#pragma once
#include "Predefined.hpp"
// 快速切换BOX设定集
#define BOX 3

// 引脚设定集，你可以为每套设备创建一个#if BOX块，记录不同设备的不同引脚信息，然后通过设定BOX宏进行快速切换。
#if BOX == 1
Pin BlueLed = 11;
Pin WaterPump = 2;
Pin CapacitorVdd = 7;
Pin CapacitorOut = 18;
Pin CD1 = 10;
Pin ActiveBuzzer = 52;
Pin AirPump = 8;
Pin PassiveBuzzer = 25;
Pin Optogenetic = 7;
#endif
#if BOX == 2
Pin BlueLed = 8;
Pin WaterPump = 2;
Pin CapacitorVdd = 7;
Pin CapacitorOut = 18;
Pin CD1 = 6;
Pin ActiveBuzzer = 22;
Pin AirPump = 12;
Pin Optogenetic = 53;
Pin PassiveBuzzer = 3;
#endif
#if BOX == 3
Pin BlueLed = 4;
Pin WaterPump = 2;
Pin CapacitorVdd = 6;
Pin CapacitorOut = 18;
Pin CD1 = 6;
Pin ActiveBuzzer = 3;
Pin AirPump = 12;
Pin Optogenetic = 7;
Pin PassiveBuzzer = 32;
#endif

/* 可以将基础模块using组合成自定义的复杂模块，可以是有参数的模板或无参数的实例。语法中的参数前缀提示参数类型，使用时不用写。例如DurationRep前缀表示模板参数是uint32_t类型，typename前缀表示参数是其它模块或类型等。基础模块介绍如下：
————————————
# 整数类模块
————————————
整数类模块自身不能执行，只能为其它模块的整数参数提供值，可以是常数或随机数。

## ConstantInteger<DurationRep Value>
表示一个常数整数。例如ConstantInteger<1000>表示常数1000

## RandomInteger<DurationRep Min, DurationRep Max, UID CustomID = UID::Module_RandomInteger>
表示一个最小Min（含）最大Max（含）的随机整数，还可以额外指定一个ID用于区分不同的实例。此模块在进程创建时提供一个随机初始值，那之后便不会再自动重新随机化，必须对其使用ModuleRandomize以更新随机数，否则每次使用时都会取到相同的值。
————————————
# 延时类模块
————————————
延时类模块的执行通常需要消耗一段可观的时间才能结束。

## Delay
等待一定延迟时间。支持多种语法：
- Delay<typename Unit, typename Value>，需要输入等待时间的单位和值，例如Delay<std::chrono::seconds,RandomInteger<5,10>>表示随机等待5~10秒
- Delay<>，无限等待。出于可读性考虑，还可以写成Delay<Infinite>。
允许的Unit包括 std::chrono::microseconds std::chrono::milliseconds std::chrono::seconds std::chrono::minutes std::chrono::hours，允许的Value只能是ConstantInteger或RandomInteger。

## RepeatEvery<typename Content, typename Unit, typename Period, typename Times = Infinite>
每隔一段时间就重复执行模块，第一次重复也需要先等待时间。此模块可用于生成音调。参数说明：
Content，要执行的内容模块。Content本身异步执行，不会占用重复周期。如果Content执行时间比重复周期还长，就会每到重复周期就自动重启，不会拖慢重复周期。
Unit，重复周期的时间单位
Period，重复周期的时间值，可以是ConstantInteger或RandomInteger。如果指定RandomInteger，只会在此模块每次开始时取一次值；重复Content的过程中，再重新随机化RandomInteger，也不会再改变重复周期，因此不能用此模块实现每次重复随机间隔，只能用Delay和Repeat组合实现。
Times，重复次数，可以是ConstantInteger或RandomInteger，或者不提供此参数则默认无限重复。

## DoubleRepeat<typename ContentA, typename ContentB, typename Unit, typename PeriodA, typename PeriodB, typename Times = Infinite>
类似于RepeatEvery，但是交替执行两个内容模块ContentA和ContentB。先等待PeriodA时间后执行ContentA，再等待PeriodB时间后执行ContentB，然后循环。Times指定的是两个内容总计执行的次数之和，而不是完整周期数，因此可以指定奇数Times以使得ContentA比ContentB多执行一次。
————————————
# 瞬时类模块
————————————
瞬时类模块执行时不需要等待时间，可以立即完成

## ModuleAbort<typename Target>
执行此模块将导致Target模块被立即放弃（包括无限执行的模块），结束执行。如果Target模块当前未在执行中，则不进行任何操作。

## ModuleRestart<typename Target>
执行此模块将导致Target模块被立即重新开始执行。如果Target模块当前未在执行中，则立即开始执行。

## ModuleRandomize<typename Target>
执行此模块将导致具有随机功能的Target模块，如RandomInteger和RandomSequential，被重新随机化。不能对非随机化模块使用此模块。

## DigitalWrite<uint8_t Pin, bool HighOrLow>
执行此模块将导致指定引脚的输出电平被设置为HIGH或LOW。

## DigitalToggle<uint8_t Pin>
执行此模块将导致指定引脚的输出电平被翻转。此模块可配合RepeatEvery模块用于输出音调。

## MonitorPin<uint8_t Pin, typename Monitor>
对指定引脚注册一个中断监听器，每当引脚电平RISING时开始执行Monitor模块。Monitor模块的执行不会打断中断触发时正在执行的模块，两者将同步执行。对此模块使用ModuleAbort以停止监视引脚，但正在执行的Monitor模块不会中止。要中止Monitor模块，请对Monitor直接使用ModuleAbort。

## SerialMessage<UID Message>
向PC端发送一个预定义的Message，通常前缀Event_表示一个事件消息，将被PC端记录；Host_表示一个主机动作消息，令PC端执行相应的动作。

## CleanWhenAbort<typename Target, typename Cleaner>
将一个Cleaner模块附加到Target模块上，监听Target的开始、重启、终止或析构，这些事件之前会先执行Cleaner，但目标模块正常结束时则不会清理。Cleaner一般应是瞬时的，如果有延时操作则不会等待其完成。
————————————
# 容器类模块
————————————
容器类模块可以包含其他模块，并控制这些模块的执行顺序。执行时间取决于所包含模块的执行时间。

## Sequential<SubModules...>
按指定顺序执行多个SubModules模块，等待前一个模块执行结束才会继续执行下一个。

## RandomSequential<typename... SubModules>
类似于Sequential，但执行顺序随机。这个随机顺序在重复运行时保持相同，要重新随机化请使用ModuleRandomize模块。此模块还支持以下扩展：
- typename RandomSequential<typename... SubModules>::template WithRepeat<uint16_t... Repeats>：指定的Repeats对应每个SubModules的重复次数，这些重复也将互相穿插随机打乱执行。例如`typename RandomSequential<A,B,C>::template WithRepeat<20,30,10>`将会把20次A、30次B、10次C模块随机穿插洗牌执行。

## Repeat<typename Content, typename Times = Infinite>
重复执行Content模块，等待前一次重复执行结束才会继续执行下一次。Times是重复次数，可以是ConstantInteger或RandomInteger，或者不提供此参数则默认无限重复；如果指定RandomInteger，将在此模块开始时确定重复次数；在重复执行过程中重新随机化RandomInteger不会改变重复次数。

## Trial<UID TrialID, typename Content>
表示一个回合。TrialID是该模块的唯一标识符。Content是回合内要执行的内容模块。回合开始时将把TrialID发往PC端进行记录并提示回合开始。
回合还是断线重连恢复执行的基本单位。断线重连后，尚未执行完毕的回合将从头开始重新执行，已经执行完毕的回合将不会重复执行。不在回合内的模块在断线重连后不会跳过，仍会重复执行。
回合内不允许嵌套回合。

## DynamicSlot<UID UniqueID = UID::Module_DynamicSlot>
表示一个动态插槽，可以在运行时动态加载、清除或切换其内容模块。UniqueID是该模块的唯一标识符。执行此模块时，将执行当前插槽内的内容模块（如果有），否则什么也不做。对此模块的重启和终止操作也将传递给当前插槽内的内容模块（如果有）。要修改插槽内容，请使用以下扩展：
- typename DynamicSlot<UniqueID>::template Load<Content>：将插槽内容设置为Content模块。如果插槽内已经有内容模块正在运行，不会终止它，换新后仍继续执行。
- typename DynamicSlot<UniqueID>::Clear：清除插槽内容。不会终止当前正在运行的内容模块，清除后仍继续执行。

## IDModule<UID ID>
使用此模块后，必须用AssignModuleID宏将某个模块绑定到TargetID上：
```
AssignModuleID(TargetModule, TargetID);
```
这样执行此模块时，将视为执行与TargetID所绑定的模块TargetModule相同的模块。此模块的重启和终止操作也将传递给TargetModule。此模块主要用于实现自我循环引用。IDModule可以在TargetModule之前声明，但AssignModuleID必须在TargetModule定义之后。

## Async<typename Content>
异步执行Content模块。执行此模块时，将立即返回并继续执行后续模块，而Content模块将在后台异步执行。此模块的Restart和Abort操作也将传递给Content模块。此模块主要用于实现后台任务。

——以下提供实际用例，用户可根据需要进行修改——
*/

template<DurationRep Milliseconds>
using DelayMilliseconds = Delay<std::chrono::milliseconds, ConstantInteger<Milliseconds>>;

template<uint8_t PinIndex, DurationRep Milliseconds>
using PinFlash = Sequential<DigitalWrite<PinIndex, HIGH>, DelayMilliseconds<Milliseconds>, DigitalWrite<PinIndex, LOW>>;

template<uint8_t PinIndex, DurationRep Milliseconds, UID Up>
using PinFlashUp = Sequential<DigitalWrite<PinIndex, HIGH>, SerialMessage<Up>, DelayMilliseconds<Milliseconds>, DigitalWrite<PinIndex, LOW>>;

template<uint8_t PinIndex, DurationRep Milliseconds, UID Up, UID Down>
using PinFlashUpDown = Sequential<DigitalWrite<PinIndex, HIGH>, SerialMessage<Up>, DelayMilliseconds<Milliseconds>, DigitalWrite<PinIndex, LOW>, SerialMessage<Down>>;

using Random100To1000 = RandomInteger<100, 1000>;

using RandomFlash = Repeat<Sequential<DigitalToggle<Optogenetic>, Delay<std::chrono::milliseconds, Random100To1000>, ModuleRandomize<Random100To1000>>>;

using Random5To10 = RandomInteger<5, 10>;

using Delay5To10 = Delay<std::chrono::seconds, Random5To10>;

using MonitorRestart = MonitorPin<CapacitorOut, ModuleRestart<Delay5To10>>;

template<DurationRep Seconds>
using DelaySeconds = Delay<std::chrono::seconds, ConstantInteger<Seconds>>;

template<DurationRep FrequencyHz, DurationRep Milliseconds>
using Tone = RepeatEvery<DigitalToggle<PassiveBuzzer>, std::chrono::microseconds, ConstantInteger<500000 / FrequencyHz>, ConstantInteger<Milliseconds * FrequencyHz / 500>>;

using ResponseWindow = MonitorPin<CapacitorOut, Sequential<DynamicSlot<>::Clear, ModuleAbort<IDModule<UID::Module_ResponseWindow>>, SerialMessage<UID::Event_MonitorHit>>>;
AssignModuleID(ResponseWindow, UID::Module_ResponseWindow);

using CalmDown = Sequential<DynamicSlot<>::Load<Sequential<ModuleAbort<ResponseWindow>, SerialMessage<UID::Event_MonitorMiss>>>, MonitorRestart, Delay5To10, ModuleAbort<MonitorRestart>>;

using Settlement = Sequential<ModuleRandomize<Random5To10>, DelaySeconds<20>>;

using Delay800ms = DelayMilliseconds<800>;

template<uint8_t CuePin, UID CueUp, UID CueDown>
using AssociationTrial = Sequential<CalmDown, ResponseWindow, PinFlashUpDown<CuePin, 200, CueUp, CueDown>, Delay800ms, DynamicSlot<>, DigitalWrite<WaterPump, HIGH>, SerialMessage<UID::Event_Water>, DelayMilliseconds<150>, DigitalWrite<WaterPump, LOW>, Settlement>;

template<typename Cue>
using CueOnlyTrial = Sequential<CalmDown, ResponseWindow, Cue, Delay800ms, DynamicSlot<>, Settlement>;

using BackgroundMonitor = MonitorPin<CapacitorOut, SerialMessage<UID::Event_HitCount>>;

// 点亮电容后等待1s，渡过刚启动的不稳定期
template<typename TrialType>
using AssociationSession = Sequential<DigitalWrite<CapacitorVdd, HIGH>, DelaySeconds<1>, BackgroundMonitor, Repeat<TrialType, ConstantInteger<30>>, ModuleAbort<BackgroundMonitor>>;

// ——以下列出所有公开模块，均绑定到ID，允许PC端调用——
std::unordered_map<UID, uint16_t (*)(Process *)> SessionMap = {
  { UID::Test_BlueLed, Session<PinFlash<BlueLed, 200>> },
  { UID::Test_WaterPump, Session<PinFlash<WaterPump, 150>> },
  { UID::Test_CapacitorReset, Session<Sequential<DigitalWrite<CapacitorVdd, LOW>, DelayMilliseconds<100>, DigitalWrite<CapacitorVdd, HIGH>>> },
  { UID::Test_CapacitorMonitor, Session<Sequential<DigitalWrite<CapacitorVdd, HIGH>, MonitorPin<CapacitorOut, SerialMessage<UID::Event_MonitorHit>>>> },
  { UID::Test_CD1, Session<PinFlash<CD1, 200>> },
  { UID::Test_ActiveBuzzer, Session<PinFlash<ActiveBuzzer, 200>> },
  { UID::Test_AirPump, Session<PinFlash<AirPump, 200>> },
  { UID::Test_Optogenetic, Session<PinFlash<Optogenetic, 200>> },
  { UID::Test_HostAction, Session<SerialMessage<UID::Host_GratingImage>> },
  { UID::Test_SquareWave, Session<DoubleRepeat<DigitalWrite<Optogenetic, HIGH>, DigitalWrite<Optogenetic, LOW>, std::chrono::seconds, ConstantInteger<1>, ConstantInteger<2>, ConstantInteger<6>>> },  // 注意是6次变灯，不是6个周期
  { UID::Test_RandomFlash, Session<Sequential<Async<RandomFlash>, DelaySeconds<10>, ModuleAbort<RandomFlash>>> },
  { UID::Test_LowTone, Session<Tone<500, 1000>> },
  { UID::Test_HighTone, Session<Tone<5000, 1000>> },
  { UID::Session_AudioWater, Session<AssociationSession<Trial<UID::Trial_AudioWater, AssociationTrial<ActiveBuzzer, UID::Event_AudioUp, UID::Event_AudioDown>>>> },
  { UID::Session_LightWater, Session<AssociationSession<Trial<UID::Trial_LightWater, AssociationTrial<BlueLed, UID::Event_LightUp, UID::Event_LightDown>>>> },
  { UID::Session_LAuW, Session<Sequential<DigitalWrite<CapacitorVdd, HIGH>, RandomSequential<
                                                                              Trial<UID::Trial_LightOnly, CueOnlyTrial<PinFlashUpDown<BlueLed, 200, UID::Event_LightUp, UID::Event_LightDown>>>,
                                                                              Trial<UID::Trial_AudioOnly, CueOnlyTrial<PinFlashUpDown<ActiveBuzzer, 200, UID::Event_AudioUp, UID::Event_AudioDown>>>,
                                                                              Trial<UID::Trial_WaterOnly, CueOnlyTrial<PinFlashUp<WaterPump, 150, UID::Event_Water>>>>::WithRepeat<20, 20, 20>>> },
};