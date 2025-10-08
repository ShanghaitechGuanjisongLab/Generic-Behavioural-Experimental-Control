#pragma once
#include "Predefined.hpp"
// 快速切换BOX设定集
#define BOX 2

// 引脚设定集，你可以为每套设备创建一个#if BOX块，记录不同设备的不同引脚信息，然后通过设定BOX宏进行快速切换。
#if BOX == 1
Pin BlueLed = 11;
Pin WaterPump = 2;
Pin CapacitorVdd = 7;
Pin CapacitorOut = 18;
Pin CD1 = 6;
Pin ActiveBuzzer = 52;
Pin AirPump = 12;
Pin Laser = 49;
Pin PassiveBuzzer = 25;
#endif
#if BOX == 2
Pin BlueLed = 8;
Pin WaterPump = 2;
Pin CapacitorVdd = 7;
Pin CapacitorOut = 18;
Pin CD1 = 6;
Pin ActiveBuzzer = 22;
Pin AirPump = 12;
Pin Laser = 53;
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
Pin Laser = 7;
Pin PassiveBuzzer = 3;
#endif

/* 可以将基础模块using组合成自定义的复杂模块，可以是有参数的模块模板或无参数的固定模块。基础模块介绍如下：
# ——延时类模块——
延时类模块的执行通常需要消耗一段可观的时间才能结束。

## Delay<Duration>
等待指定的Duration时间。Duration必须是一个时间类模块。

## RepeatEvery<Period, Content>
表示一个周期性执行的内容模块。Period是一个时间类模块，表示执行的周期；Content是要执行的内容模块。此模块还支持以下扩展：
- RepeatEvery<Period, Content>::template UntilDuration<Duration>：表示持续执行指定的Duration时间后结束。Duration必须是一个时间类模块。
- RepeatEvery<Period, Content>::template UntilTimes<Times>：表示执行指定的Times次数后结束。

## DoubleRepeat<PeriodA,ContentA,PeriodB,ContentB>
表示交替执行两个内容模块ContentA和ContentB。先等待PeriodA时间后执行ContentA，再等待PeriodB时间后执行ContentB，然后循环。PeriodA和PeriodB必须是时间类模块。此模块还支持以下扩展：
- DoubleRepeat<PeriodA,ContentA,PeriodB,ContentB>::template UntilDuration<Duration>：表示持续执行指定的Duration时间后结束。Duration必须是一个时间类模块。
- DoubleRepeat<PeriodA,ContentA,PeriodB,ContentB>::templateUntilTimes<Times>：表示执行指定的Times次ContentA后结束。这个Times是两个内容模块执行的次数之和，而非循环周期次数。因此设为奇数时，ContentA将比ContentB多执行一次。

# ——时间类模块——
时间类模块不能直接执行，需要配合延时类模块使用

## ConstantDuration<Unit, Value>
表示一个固定的时间长度，单位是Unit（如std::chrono::milliseconds），值是Value（如1000）。

## RandomDuration<Unit, Min, Max, CustomID=UID::Duration_Random>
表示一个随机的时间长度。单位是Unit（如std::chrono::milliseconds），值在Min到Max之间。CustomID是该模块的唯一标识符。随机化算法为对数均匀分布。
此模块在初始化时会随机一个时间长度，那之后就不再改变。要重新随机化，请使用ModuleRandomize模块。

## InfiniteDuration
表示一个无限的时间长度。此模块只能用于Delay。

# ——瞬时类模块——
瞬时类模块执行时不需要等待时间，可以立即完成

## ModuleAbort<Target>
执行此模块将导致Target模块被立即放弃，结束执行。如果Target模块当前未在执行中，则不进行任何操作。

## ModuleRestart<Target>
执行此模块将导致Target模块被立即重新开始执行。如果Target模块当前未在执行中，则立即开始执行。

## ModuleRandomize<Target>
执行此模块将导致Target模块的随机化参数被重新随机化。不能对非随机化模块使用此模块。

## DigitalWrite<Pin, HighOrLow>
执行此模块将导致指定引脚的输出电平被设置为高或低。

## DigitalToggle<Pin>
执行此模块将导致指定引脚的输出电平被翻转。此模块可配合RepeatEvery模块用于输出音频。

## MonitorPin<Pin, Monitor>
对指定引脚注册一个中断监听器，当引脚电平RISING时执行Monitor模块。Monitor模块的执行不会打断中断触发时正在执行的模块，两者将同步执行。对此模块使用ModuleAbort以停止监视引脚，但正在执行的Monitor模块不会中止。要中止Monitor模块，请对其直接使用ModuleAbort。

## SerialMessage<Message>
向PC端发送一个预定义的Message。Message必须是UID枚举中的一个值。通常前缀Event_表示一个事件消息，将被PC端记录；Host_表示一个主机动作消息，令PC端执行相应的动作。

## CleanWhenAbort<Target,Cleaner>
将一个Cleaner模块附加到Target模块上，开始、重启、终止或析构此模块前都将先执行清理模块，但目标模块正常结束时则不会清理。清理模块一般应是瞬时的，如果有延时操作则不会等待其完成。

# ——容器类模块——
容器类模块可以包含其他模块，并控制这些模块的执行顺序。执行时间取决于所包含模块的执行时间。

## RandomSequential<SubModules...>
按照随机顺序执行多个SubModules模块。这个随机顺序在重复运行时保持相同，要重新随机化请使用ModuleRandomize模块。此模块还支持以下扩展：
- RandomSequential<SubModules...>::template WithRepeat<Repeats...>：指定的Repeats对应每个SubModules的重复次数，这些重复也将互相穿插随机打乱执行

## Repeat<Content>
无限重复执行Content模块。要停止执行，请使用ModuleAbort模块。此模块还支持以下扩展：
- Repeat<Content>::template UntilTimes<Times>：重复执行Content模块指定的次数

## Sequential<SubModules...>
顺序执行多个SubModules模块。

## Trial<CustomID, Content>
表示一个回合。CustomID是该模块的唯一标识符。Content是回合内要执行的内容模块。回合开始时将把CustomID发往PC端进行记录并提示回合开始。
回合还是断线重连恢复执行的基本单位。断线重连后，尚未执行完毕的回合将从头开始重新执行，已经执行完毕的回合将不会重复执行。不在回合内的模块在断线重连后不会跳过，仍会重复执行。
回合内不允许嵌套回合。

## DynamicSlot<UniqueID=UID::Module_DynamicSlot>
表示一个动态插槽，可以在运行时动态加载、清除或切换其内容模块。UniqueID是该模块的唯一标识符。执行此模块时，将执行当前插槽内的内容模块（如果有），否则什么也不做。对此模块的重启和终止操作也将传递给当前插槽内的内容模块（如果有）。要修改插槽内容，请使用以下扩展：
- DynamicSlot<UniqueID>::template Load<Content>：将插槽内容设置为Content模块。如果插槽内已经有内容模块正在运行，不会终止它，换新后仍继续执行。
- DynamicSlot<UniqueID>::Clear：清除插槽内容。不会终止当前正在运行的内容模块，清除后仍继续执行。

## IDModule<TargetID>
使用此模块后，必须用AssignModuleID宏将某个模块绑定到TargetID上：
```
AssignModuleID(TargetModule, TargetID);
```
这样执行此模块时，将视为执行与TargetID所绑定的模块TargetModule相同的模块。此模块的重启和终止操作也将传递给TargetModule。此模块主要用于实现自我循环引用。

IDModule可以在TargetModule之前声明，但AssignModuleID必须在TargetModule定义之后。

## Async<Content>
异步执行Content模块。执行此模块时，将立即返回并继续执行后续模块，而Content模块将在后台异步执行。此模块的Restart和Abort操作也将传递给Content模块。此模块主要用于实现后台任务。

——以下提供实际用例，用户可根据需要进行修改——
*/

template<uint8_t PinIndex, DurationRep Milliseconds>
using PinFlash = Sequential<DigitalWrite<PinIndex, HIGH>, Delay<ConstantDuration<std::chrono::milliseconds, Milliseconds>>, DigitalWrite<PinIndex, LOW>>;

template<uint8_t PinIndex, DurationRep Milliseconds, UID Up>
using PinFlashUp = Sequential<DigitalWrite<PinIndex, HIGH>, SerialMessage<Up>, Delay<ConstantDuration<std::chrono::milliseconds, Milliseconds>>, DigitalWrite<PinIndex, LOW>>;

template<uint8_t PinIndex, DurationRep Milliseconds, UID Up, UID Down>
using PinFlashUpDown = Sequential<DigitalWrite<PinIndex, HIGH>, SerialMessage<Up>, Delay<ConstantDuration<std::chrono::milliseconds, Milliseconds>>, DigitalWrite<PinIndex, LOW>, SerialMessage<Down>>;

using Duration100To1000 = RandomDuration<std::chrono::milliseconds, 100, 1000>;

using RandomFlash = Repeat<Sequential<DigitalToggle<Laser>, Delay<Duration100To1000>, ModuleRandomize<Duration100To1000>>>;

using Duration5To10 = RandomDuration<std::chrono::seconds, 5, 10>;

using Delay5To10 = Delay<Duration5To10>;

using MonitorRestart = MonitorPin<CapacitorOut, ModuleRestart<Delay5To10>>;

template<DurationRep Milliseconds>
using DelayMilliseconds = Delay<ConstantDuration<std::chrono::milliseconds, Milliseconds>>;

template<DurationRep Seconds>
using DelaySeconds = Delay<ConstantDuration<std::chrono::seconds, Seconds>>;

template<DurationRep FrequencyHz, DurationRep Milliseconds>
using Tone = typename RepeatEvery<ConstantDuration<std::chrono::microseconds, 1000000 / FrequencyHz>, DigitalToggle<PassiveBuzzer>>::template UntilDuration<ConstantDuration<std::chrono::microseconds, Milliseconds * 1000>>;

using ResponseWindow = MonitorPin<CapacitorOut, Sequential<DynamicSlot<>::Clear, ModuleAbort<IDModule<UID::Module_ResponseWindow>>, SerialMessage<UID::Event_MonitorHit>>>;
AssignModuleID(ResponseWindow, UID::Module_ResponseWindow);

using CalmDown = Sequential<DynamicSlot<>::Load<Sequential<ModuleAbort<ResponseWindow>, SerialMessage<UID::Event_MonitorMiss>>>, MonitorRestart, Delay5To10, ModuleAbort<MonitorRestart>>;

using Settlement = Sequential<ModuleRandomize<Duration5To10>, DelaySeconds<20>>;

using Delay800ms = DelayMilliseconds<800>;

template<uint8_t CuePin, UID CueUp, UID CueDown>
using AssociationTrial = Sequential<CalmDown, ResponseWindow, PinFlashUpDown<CuePin, 200, CueUp, CueDown>, Delay800ms, DynamicSlot<>, PinFlashUp<WaterPump, 150, UID::Event_Water>, Settlement>;

template<typename Cue>
using CueOnlyTrial = Sequential<CalmDown, ResponseWindow, Cue, Delay800ms, DynamicSlot<>, Settlement>;

using CapacitorInitialize = Sequential<DigitalWrite<CapacitorVdd, HIGH>, DelaySeconds<1>, MonitorPin<CapacitorOut, SerialMessage<UID::Event_HitCount>>>;

// 点亮电容后等待1s，渡过刚启动的不稳定期
template<typename TrialType>
using AssociationSession = Sequential<CapacitorInitialize, typename Repeat<TrialType>::template UntilTimes<30>>;

// ——以下列出所有公开模块，均绑定到ID，允许PC端调用——
std::unordered_map<UID, uint16_t (*)(Process *)> SessionMap = {
  { UID::Test_BlueLed, Session<PinFlash<BlueLed, 200>> },
  { UID::Test_WaterPump, Session<PinFlash<WaterPump, 150>> },
  { UID::Test_CapacitorReset, Session<Sequential<DigitalWrite<CapacitorVdd, LOW>, Delay<ConstantDuration<std::chrono::milliseconds, 100>>, DigitalWrite<CapacitorVdd, HIGH>>> },
  { UID::Test_CapacitorMonitor, Session<Sequential<DigitalWrite<CapacitorVdd, HIGH>, MonitorPin<CapacitorOut, SerialMessage<UID::Event_MonitorHit>>>> },
  { UID::Test_CD1, Session<PinFlash<CD1, 200>> },
  { UID::Test_ActiveBuzzer, Session<PinFlash<ActiveBuzzer, 200>> },
  { UID::Test_AirPump, Session<PinFlash<AirPump, 200>> },
  { UID::Test_HostAction, Session<SerialMessage<UID::Host_GratingImage>> },
  { UID::Test_SquareWave, Session<DoubleRepeat<ConstantDuration<std::chrono::seconds, 1>, DigitalWrite<Laser, HIGH>, ConstantDuration<std::chrono::seconds, 2>, DigitalWrite<Laser, LOW>>::template UntilTimes<6>> },  // 注意是6次变灯，不是6个周期
  { UID::Test_RandomFlash, Session<Sequential<Async<RandomFlash>, Delay<ConstantDuration<std::chrono::seconds, 10>>, ModuleAbort<RandomFlash>>> },
  { UID::Test_LowTone, Session<Tone<500, 1000>> },
  { UID::Test_HighTone, Session<Tone<5000, 1000>> },
  { UID::Session_AudioWater, Session<AssociationSession<Trial<UID::Trial_AudioWater, AssociationTrial<ActiveBuzzer, UID::Event_AudioUp, UID::Event_AudioDown>>>> },
  { UID::Session_LightWater, Session<AssociationSession<Trial<UID::Trial_LightWater, AssociationTrial<BlueLed, UID::Event_LightUp, UID::Event_LightDown>>>> },
  { UID::Session_LAuW, Session<Sequential<DigitalWrite<CapacitorVdd, HIGH>, RandomSequential<
                                                                              Trial<UID::Trial_LightOnly, CueOnlyTrial<PinFlashUpDown<BlueLed, 200, UID::Event_LightUp, UID::Event_LightDown>>>,
                                                                              Trial<UID::Trial_AudioOnly, CueOnlyTrial<PinFlashUpDown<ActiveBuzzer, 200, UID::Event_AudioUp, UID::Event_AudioDown>>>,
                                                                              Trial<UID::Trial_WaterOnly, CueOnlyTrial<PinFlashUp<WaterPump, 150, UID::Event_Water>>>>::WithRepeat<20, 20, 20>>> },

};
