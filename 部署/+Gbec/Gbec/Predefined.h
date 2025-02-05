#pragma once
#include "ISession.h"
#include "SerialIO.h"
#include <Timers_one_for_all.hpp>
#include <Low_level_quick_digital_IO.hpp>
#include <algorithm>
#include <map>
#include <numeric>
#include <set>
#include <type_traits>
#include <cmath>
#include <unordered_map>
template<uint8_t Pin>
std::set<std::move_only_function<void()const>const*> CallbackSet;
constexpr bool PinInterruptable(uint8_t Pin) {
  return digitalPinToInterrupt(Pin) != NOT_AN_INTERRUPT;
}
extern UID State;
template<uint8_t Pin>
void TraverseCallback() {
  if (State != State_SessionPaused)
    for (std::move_only_function<void()const>const* const C : CallbackSet<Pin>)
      (*C)();
}
template<uint8_t Pin>
bool NeedSetup = true;

// 如果试图添加重复的中断回调，则不会添加，也不会出错
template<uint8_t Pin>
void RisingInterrupt(std::move_only_function<void()const>const* Callback) {
  if (CallbackSet<Pin>.empty()) {
    // 清除attachInterrupt之前已经立起并被记住的中断旗帜。中断旗帜与引脚号的对应关系与digitalPinToInterrupt不同。
#ifdef ARDUINO_ARCH_AVR
    Low_level_quick_digital_IO::ClearInterrupt<Pin>();
#elif defined(ARDUINO_ARCH_SAM)
    Low_level_quick_digital_IO::ClearInterruptPending<Pin>();
#else
#error 未知的芯片类型
#endif
    Low_level_quick_digital_IO::AttachInterrupt<Pin, RISING>(TraverseCallback<Pin>);
  }
  CallbackSet<Pin>.insert(Callback);
}

template<uint8_t Pin>
void DetachInterrupt(std::move_only_function<void()const>const* Callback) {
  CallbackSet<Pin>.erase(Callback);
  if (CallbackSet<Pin>.empty())
    // detachInterrupt不能阻止后续中断旗帜再被立起
    Low_level_quick_digital_IO::DetachInterrupt<Pin>();
}
struct ITest {
  UID MyUID;
  // 测试开始时将调用此方法。测试分为自动结束型和手动结束型。对于自动结束型，应当根据TestTimes参数将测试重复指定的次数，并返回true表示该测试将自动结束。对于手动结束型，一般应当忽略TestTimes参数，返回false，持续测试直到Stop被调用。
  virtual bool Start(uint16_t TestTimes) const = 0;
  // 测试被用户手动结束时将调用此方法。自动结束型测试无需实现此方法，手动结束型则必须实现。
  virtual void Stop() const {}
  constexpr ITest(UID MyUID)
    : MyUID(MyUID) {
  }
};
// 给引脚一段时间的高或低电平，然后反转
template<UID TMyUID, uint8_t Pin, uint16_t Milliseconds, bool HighOrLow = HIGH>
struct PinFlashTest : public ITest {
  constexpr PinFlashTest()
    : ITest(TMyUID) {
  }
  bool Start(uint16_t TestTimes) const override {
    if (NeedSetup<Pin>) {
      pinMode(Pin, OUTPUT);
      NeedSetup<Pin> = false;
    }
    Low_level_quick_digital_IO::DigitalWrite<Pin, HighOrLow>();
    Timers_one_for_all::TimerClass const* const Timer = Timers_one_for_all::AllocateTimer();
    constexpr std::chrono::milliseconds Delay{ Milliseconds };
    Timer->DoAfter(Delay * TestTimes, [Timer]() {
      Timer->Allocatable(true);
      Low_level_quick_digital_IO::DigitalToggle<Pin>();
      });
    return true;
  }
};
template<uint16_t Min, uint16_t Max>
uint16_t LogRandom() {
  static const float LogMin = std::log2(Min);
  static const float LogRange = std::log2(Max) - LogMin;
  return pow(2, random(Max - Min) * LogRange / (Max - Min) + LogMin);
}
template<uint16_t HighMilliseconds, uint16_t LowMilliseconds, uint16_t RandomCycleMin, uint16_t RandomCycleMax>
void PopulateRandomCycles(std::vector<std::chrono::milliseconds>& FlashCycles, uint8_t Times = 1) {
  static constexpr uint16_t FullMilliseconds = HighMilliseconds + LowMilliseconds;
  static constexpr float HighRatio = static_cast<float>(HighMilliseconds) / FullMilliseconds;
  FlashCycles.clear();
  uint16_t MillisecondsLeft = FullMilliseconds * Times;
  while (MillisecondsLeft) {
    uint16_t RandomCycle = LogRandom<RandomCycleMin, RandomCycleMax>();
    if (RandomCycle + RandomCycleMin > MillisecondsLeft)
      RandomCycle = RandomCycle < MillisecondsLeft ? (RandomCycleMax < MillisecondsLeft ? MillisecondsLeft - RandomCycleMin : (RandomCycle < MillisecondsLeft + MillisecondsLeft - RandomCycleMin ? MillisecondsLeft - RandomCycleMin : MillisecondsLeft)) : MillisecondsLeft;
    MillisecondsLeft -= RandomCycle;
    const float FloatHigh = RandomCycle * HighRatio;
    uint16_t IntegerHigh = FloatHigh;
    if (IntegerHigh & 1) {
      if (FloatHigh >= IntegerHigh + 0.5)
        IntegerHigh++;
    }
    else {
      if (FloatHigh > IntegerHigh + 0.5)
        IntegerHigh++;
    }
    FlashCycles.emplace_back(IntegerHigh);
    FlashCycles.emplace_back(RandomCycle - IntegerHigh);
  }
}
// 给引脚一段时间的高或低电平，然后反转
template<UID TMyUID, uint8_t Pin, uint16_t HighMilliseconds, uint16_t LowMilliseconds, uint16_t RandomCycleMin, uint16_t RandomCycleMax>
struct RandomFlashTest : public ITest {
  constexpr RandomFlashTest()
    : ITest(TMyUID) {
  }
  Timers_one_for_all::TimerClass const* Timer;
  std::vector<std::chrono::milliseconds> FlashCycles;
  std::vector<std::chrono::milliseconds>::const_iterator NextCycle;
  bool Start(uint16_t TestTimes) const override {
    if (NeedSetup<Pin>) {
      pinMode(Pin, OUTPUT);
      NeedSetup<Pin> = false;
    }
    if (HighMilliseconds) {
      PopulateRandomCycles<HighMilliseconds, LowMilliseconds, RandomCycleMin, RandomCycleMax>(FlashCycles);
      NextCycle = FlashCycles.cbegin();
      Timer = Timers_one_for_all::AllocateTimer();
      SetHigh();
    }
    return true;
  }

protected:
  void SetHigh() {
    if (NextCycle < FlashCycles.cend()) {
      Low_level_quick_digital_IO::DigitalWrite<Pin, HIGH>();
      Timer->DoAfter(*(NextCycle++), [this]() {this->SetLow(); });
    }
    else
      Timer->Allocatable(true);
  }
  void SetLow() {
    Low_level_quick_digital_IO::DigitalWrite<Pin, LOW>();
    Timer->DoAfter(*(NextCycle++), [this]() {this->SetHigh(); });
  }
};
// 监视引脚，每次高电平发送串口报告。此测试需要调用Stop才能终止，且无视TestTimes参数
template<UID TMyUID, uint8_t Pin>
class MonitorTest : public ITest {
  static void ReportHit() {
    SerialWrite(Signal_MonitorHit);
  }

public:
  constexpr MonitorTest()
    : ITest(TMyUID) {
  }
  bool Start(uint16_t) const override {
    static_assert(PinInterruptable(Pin), "试图将不支持的引脚用于MonitorTest");
    if (NeedSetup<Pin>) {
      pinMode(Pin, INPUT);
      NeedSetup<Pin> = false;
    }
    RisingInterrupt<Pin>(ReportHit);
    return false;
  }
  void Stop() const override {
    DetachInterrupt<Pin>(ReportHit);
  }
};
// 测试具有指定高电平和低电平毫秒数和循环次数的方波
template<UID TMyUID, uint8_t Pin, uint16_t HighMilliseconds, uint16_t LowMilliseconds, uint16_t NumCycles>
struct SquareWaveTest : public ITest {
  constexpr SquareWaveTest()
    : ITest(TMyUID) {
  }
  Timers_one_for_all::TimerClass const* Timer;
  bool Start(uint16_t TestTimes) const override {
    if (NeedSetup<Pin>) {
      pinMode(Pin, OUTPUT);
      NeedSetup<Pin> = false;
    }
    Timer = Timers_one_for_all::AllocateTimer();
    Timer->DoubleRepeat(std::chrono::milliseconds{ HighMilliseconds }, Low_level_quick_digital_IO::DigitalToggle<Pin>, std::chrono::milliseconds{ LowMilliseconds }, Low_level_quick_digital_IO::DigitalToggle<Pin>, NumCycles * TestTimes * 2, [Timer]() {Timer->Allocatable(true);});
    return true;
  }
};
// 测试具有指定高电平和低电平毫秒数和循环次数的方波
template<UID TMyUID, uint8_t Pin, uint32_t FrequencyHz, uint16_t Milliseconds>
struct ToneTest : public ITest {
  constexpr ToneTest()
    : ITest(TMyUID) {
  }
  bool Start(uint16_t TestTimes) const override {
    if (NeedSetup<Pin>) {
      pinMode(Pin, OUTPUT);
      NeedSetup<Pin> = false;
    }
    Timer = Timers_one_for_all::AllocateTimer();
    Timer->RepeatEvery(std::chrono::microseconds{ 1000000 / FrequencyHz }, Low_level_quick_digital_IO::DigitalToggle<Pin>, std::chrono::milliseconds{ Milliseconds * TestTimes }, [Timer]() {Timer->Allocatable(true);});
    return true;
  }
};
template<typename T>
constexpr T Instance;
template<typename... Ts>
const std::map<UID, const ITest*> TestMap_t{ { Instance<Ts>.MyUID, &Instance<Ts> }... };
template<typename ToAdd, typename Container>
struct _AddToArray;
template<typename ToAdd, template<typename...> typename Container, typename... AlreadyIn>
struct _AddToArray<ToAdd, Container<AlreadyIn...>> {
  using Result = Container<ToAdd, AlreadyIn...>;
};
template<typename ToAdd, typename Container>
using AddToArray = typename _AddToArray<ToAdd, Container>::Result;
template<typename T>
constexpr UID TypeToUID = T::MyUID;
template<>
constexpr UID TypeToUID<bool> = Type_Bool;
template<>
constexpr UID TypeToUID<uint8_t> = Type_UInt8;
template<>
constexpr UID TypeToUID<uint16_t> = Type_UInt16;
template<>
constexpr UID TypeToUID<UID> = Type_UID;
#pragma pack(push, 1)
template<typename T, typename... Ts>
struct InfoArray {
  constexpr static UID MyUID = Type_Array;
  uint8_t Number = sizeof...(Ts) + 1;
  UID Type = TypeToUID<typename T::value_type>;
  typename T::value_type Array[sizeof...(Ts) + 1] = { T::value, Ts::value... };
};
template<typename T, typename... Ts>
struct CellArray {
  UID Type = TypeToUID<T>;
  T Value;
  CellArray<Ts...> Values;
  constexpr CellArray(T Value, Ts... Values)
    : Value(Value), Values(CellArray<Ts...>(Values...)) {
  }
};
template<typename T>
struct CellArray<T> {
  UID Type = TypeToUID<T>;
  T Value;
  constexpr CellArray(T Value)
    : Value(Value) {
  }
};
template<typename... T>
struct InfoCell {
  constexpr static UID MyUID = Type_Cell;
  uint8_t NumCells = 0;
};
template<typename T, typename... Ts>
struct InfoCell<T, Ts...> {
  constexpr static UID MyUID = Type_Cell;
  uint8_t NumCells = sizeof...(Ts) + 1;
  CellArray<T, Ts...> Values;
  constexpr InfoCell(T Value, Ts... Values)
    : Values(CellArray<T, Ts...>(Value, Values...)) {
  }
};
// 只有主模板能推断，特化模板必须加推断向导
template<typename T, typename... Ts>
InfoCell(T, Ts...) -> InfoCell<T, Ts...>;
template<typename TName, typename TValue, typename... Ts>
struct InfoFields {
  TName Name;
  UID Type = TypeToUID<TValue>;
  TValue Value;
  InfoFields<Ts...> FollowingFields;
  constexpr InfoFields(TName Name, TValue Value, Ts... NameValues)
    : Name(Name), Value(Value), FollowingFields(InfoFields<Ts...>(NameValues...)) {
  }
};
template<typename TName, typename TValue>
struct InfoFields<TName, TValue> {
  TName Name;
  UID Type = TypeToUID<TValue>;
  TValue Value;
  constexpr InfoFields(TName Name, TValue Value)
    : Name(Name), Value(Value) {
  }
};
template<typename... Ts>
struct InfoStruct {
  constexpr static UID MyUID = Type_Struct;
  uint8_t NumFields = 0;
};
template<typename T, typename... Ts>
struct InfoStruct<UID, T, Ts...> {
  constexpr static UID MyUID = Type_Struct;
  uint8_t NumFields = sizeof...(Ts) / 2 + 1;
  InfoFields<UID, T, Ts...> Fields;
  constexpr InfoStruct(UID Name, T Value, Ts... NameValues)
    : Fields(InfoFields<UID, T, Ts...>(Name, Value, NameValues...)) {
  }
};
// 只有主模板能推断，特化模板必须加推断向导
template<typename... Ts>
InfoStruct(UID, Ts...) -> InfoStruct<UID, Ts...>;
#pragma pack(pop)
using NullStep = IStep;
// 持续等待，直到指定引脚在指定连续毫秒数内都保持静默才结束
template<uint8_t Pin, uint16_t MinMilliseconds, uint16_t MaxMilliseconds = MinMilliseconds, UID MyUID = Step_Calmdown>
class CalmdownStep : public IStep {
  static constexpr bool RandomTime = MinMilliseconds < MaxMilliseconds;
  Timers_one_for_all::TimerClass const* Timer;
  std::move_only_function<void()const>const* FinishCallback;
  std::chrono::milliseconds CurrentDuration{ MinMilliseconds };
  std::move_only_function<void()const>const Reset = [this]() {
    Timer->DoAfter(CurrentDuration, &TimeUp);
    };
  std::move_only_function<void()const>const TimeUp = [this]() {
    DetachInterrupt<Pin>(&Reset);
    Timer->Allocatable(true);
    (*FinishCallback)();
    noInterrupts()
    };

public:
  bool Start(std::move_only_function<void()const>const* FC) const override {
    FinishCallback = FC;
    if (RandomTime)
      CurrentDuration = std::chrono::milliseconds{ LogRandom<MinMilliseconds, MaxMilliseconds>() };
    Timer = TimersOneForAll::AllocateTimer();
    Timer->DoAfter(CurrentDuration, &TimeUp);
    RisingInterrupt<Pin>(&Reset);
    return true;
  }
  void Pause() const override {
    Timer->Pause();
    DetachInterrupt<Pin>(&Reset);
  }
  void Continue() const override {
    RisingInterrupt<Pin>(&Reset);
    Timer->Continue();
  }
  void Abort() const override {
    DetachInterrupt<Pin>(Reset);
    TimersOneForAll::ShutDown<TimerCode>();
  }
  void Setup() const override {
    if (NeedSetup<Pin>) {
      static_assert(PinInterruptable(Pin), "试图将不支持中断的引脚用于CalmdownStep");
      pinMode(Pin, INPUT);
      NeedSetup<Pin> = false;
    }
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_MinMilliseconds, MinMilliseconds, Info_MaxMilliseconds, MaxMilliseconds);
};
template<typename Reporter>
inline void Report() {
  Instance<Reporter>.Start([]() {});
}
// 让引脚高电平一段时间再回到低电平。异步执行，不阻塞时相
template<uint8_t Pin, uint8_t TimerCode, uint16_t Milliseconds, typename UpReporter = NullStep, typename DownReporter = NullStep, UID MyUID = Step_PinFlash>
class PinFlashStep : public IStep {
  static void DownReport() {
    DigitalWrite<Pin, LOW>();
    Report<DownReporter>();
  }

public:
  bool Start(void (*FC)()) const override {
    Report<UpReporter>();
    DigitalWrite<Pin, HIGH>();
    TimersOneForAll::DoAfter<TimerCode, Milliseconds>(DownReport);
    return false;
  }
  void Setup() const override {
    if (NeedSetup<Pin>) {
      pinMode(Pin, OUTPUT);
      NeedSetup<Pin> = false;
    }
    Instance<UpReporter>.Setup();
    Instance<DownReporter>.Setup();
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_Milliseconds, Milliseconds);
};
// 在一段时间内随机闪烁引脚。指定高电平和低电平总时长，以及每次闪烁的随机范围。
template<uint8_t Pin, uint8_t TimerCode, uint16_t HighMilliseconds, uint16_t LowMilliseconds, uint16_t RandomCycleMin, uint16_t RandomCycleMax, typename UpReporter = NullStep, typename DownReporter = NullStep, bool ReportEachCycle = false, UID MyUID = Step_RandomFlash>
class RandomFlashStep : public IStep {
  static void SetHigh() {
    if (CurrentCycle<TimerCode> < CycleEnd<TimerCode>) {
      if (ReportEachCycle)
        Report<UpReporter>();
      DigitalWrite<Pin, HIGH>();
      TimersOneForAll::DoAfter<TimerCode>(*(CurrentCycle<TimerCode>++), SetLow);
    }
  }
  static void SetLow() {
    if (ReportEachCycle)
      Report<DownReporter>();
    DigitalWrite<Pin, LOW>();
    TimersOneForAll::DoAfter<TimerCode>(*(CurrentCycle<TimerCode>++), SetHigh);
  }

public:
  bool Start(void (*FC)()) const override {
    if (!ReportEachCycle)
      Report<UpReporter>();
    if (HighMilliseconds) {
      PopulateRandomCycles<TimerCode, HighMilliseconds, LowMilliseconds, RandomCycleMin, RandomCycleMax>();
      SetHigh();
    }
    else if (!ReportEachCycle)
      Report<DownReporter>();
    return false;
  }
  void Setup() const override {
    if (NeedSetup<Pin>) {
      pinMode(Pin, OUTPUT);
      NeedSetup<Pin> = false;
    }
    Instance<UpReporter>.Setup();
    Instance<DownReporter>.Setup();
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_HighMilliseconds, HighMilliseconds, Info_LowMilliseconds, LowMilliseconds, Info_RandomCycleMin, RandomCycleMin, Info_RandomCycleMax, RandomCycleMax);
};

// 后台无限随机闪烁引脚，直到StopRandomFlash。指定每个周期的高电平和低电平时长范围。
template<uint8_t Pin, uint8_t TimerCode, uint16_t MinHighMilliseconds, uint16_t MaxHighMilliseconds, uint16_t MinLowMilliseconds, uint16_t MaxLowMilliseconds, UID MyUID = Step_StartRandomFlash>
class StartRandomFlash : public IStep {
  static void SetHigh() {
    DigitalWrite<Pin, HIGH>();
    TimersOneForAll::DoAfter<TimerCode>(LogRandom<MinHighMilliseconds, MaxHighMilliseconds>(), SetLow);
  }
  static void SetLow() {
    DigitalWrite<Pin, LOW>();
    TimersOneForAll::DoAfter<TimerCode>(LogRandom<MinLowMilliseconds, MaxLowMilliseconds>(), SetHigh);
  }

public:
  bool Start(void (*FC)()) const override {
    SetHigh();
    return false;
  }
  void Setup() const override {
    if (NeedSetup<Pin>) {
      pinMode(Pin, OUTPUT);
      NeedSetup<Pin> = false;
    }
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_MinHighMilliseconds, MinHighMilliseconds, Info_MaxHighMilliseconds, MaxHighMilliseconds, Info_MinLowMilliseconds, MinLowMilliseconds, Info_MaxLowMilliseconds, MaxLowMilliseconds);
};

// 停止后台无限随机闪烁引脚
template<uint8_t Pin, uint8_t TimerCode, UID MyUID = Step_StopRandomFlash>
class StopRandomFlash : public IStep {
public:
  bool Start(void (*FC)()) const override {
    TimersOneForAll::ShutDown<TimerCode>();
    DigitalWrite<Pin, LOW>();
    return false;
  }
  void Setup() const override {
    if (NeedSetup<Pin>) {
      pinMode(Pin, OUTPUT);
      NeedSetup<Pin> = false;
    }
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin);
};

// 在一段时间内同步监视引脚，发现高电平立即汇报。HitReporter和MissReporter都是IStep类型，一律异步执行，不等待
template<uint8_t Pin, uint8_t TimerCode, uint16_t Milliseconds, uint8_t Flags, typename HitReporter, typename MissReporter = NullStep, UID MyUID = Step_Monitor>
class MonitorStep : public IStep {
  static constexpr bool ReportOnce = Flags & Monitor_ReportOnce;
  static constexpr bool ReportMiss = !std::is_same_v<MissReporter, NullStep>;
  static constexpr bool HitAndFinish = Flags & Monitor_HitAndFinish;
  static void HitReport() {
    Report<HitReporter>();
    if (HitAndFinish) {
      DetachInterrupt<Pin>(HitReport);
      TimersOneForAll::ShutDown<TimerCode>();
      FinishCallback<TimerCode>();
    }
    else {
      if (ReportOnce)
        DetachInterrupt<Pin>(HitReport);
      if (ReportMiss)
        NoHits<TimerCode> = false;
    }
  }
  static void TimeUp() {
    DetachInterrupt<Pin>(HitReport);
    if (ReportMiss && NoHits<TimerCode>)
      Report<MissReporter>();
    FinishCallback<TimerCode>();
  }

public:
  bool Start(void (*FC)()) const override {
    FinishCallback<TimerCode> = FC;
    if (ReportMiss)
      NoHits<TimerCode> = true;
    RisingInterrupt<Pin>(HitReport);
    TimersOneForAll::DoAfter<TimerCode, Milliseconds>(TimeUp);
    return true;
  }
  void Pause() const override {
    DetachInterrupt<Pin>(HitReport);
    TimersOneForAll::Pause<TimerCode>();
  }
  void Continue() const override {
    TimersOneForAll::Continue<TimerCode>();
    RisingInterrupt<Pin>(HitReport);
  }
  void Abort() const override {
    DetachInterrupt<Pin>(HitReport);
    TimersOneForAll::ShutDown<TimerCode>();
  }
  void Setup() const override {
    if (NeedSetup<Pin>) {
      static_assert(PinInterruptable(Pin), "试图将不支持中断的引脚用于MonitorStep");
      pinMode(Pin, INPUT);
      NeedSetup<Pin> = false;
    }
    Instance<HitReporter>.Setup();
    Instance<MissReporter>.Setup();
  }
  static constexpr auto Info = InfoStruct(
    Info_UID, MyUID,
    Info_Pin, Pin,
    Info_Milliseconds, Milliseconds,
    Info_ReportOnce, ReportOnce,
    Info_ReportMiss, ReportMiss,
    Info_HitAndFinish, HitAndFinish,
    Info_HitReporter, HitReporter::Info,
    Info_MissReporter, MissReporter::Info);
};
// 不做任何事，同步等待一段时间后再进入下一步。
template<uint8_t TimerCode, uint16_t MinMilliseconds, uint16_t MaxMilliseconds = MinMilliseconds, UID MyUID = Step_Wait>
struct WaitStep : public IStep {
  bool Start(void (*FinishCallback)()) const override {
    if (MinMilliseconds < MaxMilliseconds)
      TimersOneForAll::DoAfter<TimerCode>(LogRandom<MinMilliseconds, MaxMilliseconds>(), FinishCallback);
    else
      TimersOneForAll::DoAfter<TimerCode, MinMilliseconds>(FinishCallback);
    return true;
  }
  void Pause() const override {
    TimersOneForAll::Pause<TimerCode>();
  }
  void Continue() const override {
    TimersOneForAll::Continue<TimerCode>();
  }
  void Abort() const override {
    TimersOneForAll::ShutDown<TimerCode>();
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_MinMilliseconds, MinMilliseconds, Info_MaxMilliseconds, MaxMilliseconds);
};
// 开始监视指定引脚，发现高电平立即汇报。异步执行，步骤本身立即结束，但会在后台持续监视，直到StopMonitorStep才会结束监视。
template<uint8_t Pin, typename Reporter, UID MyUID = Step_StartMonitor>
struct StartMonitorStep : public IStep {
  bool Start(void (*)()) const override {
    RisingInterrupt<Pin>(Report<Reporter>);
    return false;
  }
  void Setup() const override {
    if (NeedSetup<Pin>) {
      static_assert(PinInterruptable(Pin), "试图将不支持中断的引脚用于StartMonitorStep");
      pinMode(Pin, INPUT);
      NeedSetup<Pin> = false;
    }
    Instance<Reporter>.Setup();
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_Reporter, Reporter::Info);
};
// 停止监视指定引脚。如果一个引脚被多个Reporter监视，此步骤只会终止指定Reporter的监视，其它Reporter照常工作。
template<uint8_t Pin, typename Reporter, UID MyUID = Step_StopMonitor>
struct StopMonitorStep : public IStep {
  bool Start(void (*)()) const override {
    DetachInterrupt<Pin>(Report<Reporter>);
    return false;
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_Reporter, Reporter::Info);
  // 此类不需要Setup，初始化应由与其配对的StartMonitorStep负责
};
// 向串口写出一个字节
template<UID ToWrite, UID MyUID = Step_SerialWrite>
struct SerialWriteStep : public IStep {
  bool Start(void (*)()) const override {
    SerialWrite(ToWrite);
    return false;
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_ToWrite, ToWrite);
};
// 以精确的毫秒数记录事件。注意，如果中途发生断线重连事件，事件前后的相对时间将不再精确。
extern uint32_t TimeShift;
template<uint8_t TimerCode, UID Event, UID MyUID = Step_PreciseLog>
struct PreciseLogStep : public IStep {
  void Setup() const override {
    // 必须每次Setup，否则无法重复使用。没有办法还原“已Setup”标志位。
    TimersOneForAll::StartTiming<TimerCode>();
    TimersOneForAll::MillisecondsElapsed<TimerCode> = TimeShift;
  }
  bool Start(void (*)()) const override {
    SerialWrite(Signal_PreciseLog);
    SerialWrite(Event);
    SerialWrite(TimersOneForAll::MillisecondsElapsed<TimerCode>);
    return false;
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Event, Event);
};
// 播放具有指定频率㎐和指定毫秒数的声音。异步执行，步骤不阻塞时相。
template<uint8_t Pin, uint8_t TimerCode, uint16_t FrequencyHz, uint16_t Milliseconds, typename UpReporter = NullStep, typename DownReporter = NullStep, UID MyUID = Step_Audio>
struct ToneStep : public IStep {
  bool Start(void (*)()) const override {
    Report<UpReporter>();
    TimersOneForAll::PlayTone<TimerCode, Pin, FrequencyHz, Milliseconds, DoneCallback>();
    return false;
  }
  void Setup() const override {
    if (NeedSetup<Pin>) {
      pinMode(Pin, OUTPUT);
      NeedSetup<Pin> = false;
    }
    Instance<UpReporter>.Setup();
    Instance<DownReporter>.Setup();
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_FrequencyHz, FrequencyHz, Info_Milliseconds, Milliseconds);

protected:
  static void DoneCallback() {
    DigitalWrite<Pin, LOW>();
    Report<DownReporter>();
  }
};
// 播放具有指定高电平和低电平毫秒数的方波。异步执行，步骤不阻塞时相。
template<uint8_t Pin, uint8_t TimerCode, uint16_t HighMilliseconds, uint16_t LowMilliseconds, uint16_t NumCycles, typename UpReporter = NullStep, typename DownReporter = NullStep, UID MyUID = Step_SquareWave>
struct SquareWaveStep : public IStep {
  bool Start(void (*)()) const override {
    Report<UpReporter>();
    TimersOneForAll::SquareWave<TimerCode, Pin, HighMilliseconds, LowMilliseconds, NumCycles, Report<DownReporter>>();
    return false;
  }
  void Setup() const override {
    if (NeedSetup<Pin>) {
      pinMode(Pin, OUTPUT);
      NeedSetup<Pin> = false;
    }
    Instance<UpReporter>.Setup();
    Instance<DownReporter>.Setup();
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_HighMilliseconds, HighMilliseconds, Info_LowMilliseconds, LowMilliseconds, Info_NumCycles, NumCycles);
};
template<typename... SubSteps>
struct IndividualThreadStep : IStep {
  static constexpr const IStep* StepPointers[] = { &Instance<SubSteps>... };
  static const IStep* const* CurrentStep;
  void Setup() const override {
    for (const IStep* S : StepPointers)
      S->Setup();
  }
  static void ContinueCycle() {
    while (CurrentStep < std::end(StepPointers) && !(*CurrentStep++)->Start(ContinueCycle))
      ;
  }
  bool Start(void (*FC)()) const override {
    for (CurrentStep = StepPointers; CurrentStep < std::end(StepPointers) && !(*CurrentStep++)->Start(ContinueCycle);)
      ;
    return false;
  }
  static constexpr auto Info = InfoStruct(Info_UID, Step_IndividualThread, Info_SubSteps, InfoCell(SubSteps::Info...));
};
template<typename... SubSteps>
const IStep* const* IndividualThreadStep<SubSteps...>::CurrentStep;
template<UID TUID, typename... TSteps>
class Trial : public ITrial {
  static const IStep* Steps[sizeof...(TSteps)];
  static void NextStep() {
    while (StepsDone < sizeof...(TSteps))
      if (Steps[StepsDone++]->Start(NextStep))
        return;
    FinishCallback();
  }

public:
  static constexpr auto Info = InfoStruct(Info_UID, TUID, Info_Steps, InfoCell(TSteps::Info...));
  constexpr Trial()
    : ITrial(TUID) {
  }
  void Setup() const override {
    for (const IStep* S : Steps)
      S->Setup();
  }
  bool Start(void (*FC)()) const override {
    FinishCallback = FC;
    StepsDone = 0;
    while (StepsDone < sizeof...(TSteps))
      if (Steps[StepsDone++]->Start(NextStep))
        return true;
    return false;
  }
  void Pause() const override {
    Steps[StepsDone - 1]->Pause();
  }
  void Continue() const override {
    Steps[StepsDone - 1]->Continue();
  }
  void Abort() const override {
    Steps[StepsDone - 1]->Abort();
  }
};
template<UID TUID, typename... TSteps>
const IStep* Trial<TUID, TSteps...>::Steps[sizeof...(TSteps)] = { &Instance<TSteps>... };
template<uint16_t Value>
using N = std::integral_constant<uint16_t, Value>;
template<typename... Trials>
struct TrialArray {
  static const ITrial* Interfaces[sizeof...(Trials)];
  static constexpr auto Info = InfoCell(Trials::Info...);
};
// 静态成员必须类外定义，即使模板也一样
template<typename... Trials>
const ITrial* TrialArray<Trials...>::Interfaces[sizeof...(Trials)] = { &Instance<Trials>... };
template<typename TTrial, typename TNumber, typename... TrialThenNumber>
struct TrialNumberSplit {
  using Numbers_t = AddToArray<TNumber, typename TrialNumberSplit<TrialThenNumber...>::Numbers_t>;
  using Trials_t = AddToArray<TTrial, typename TrialNumberSplit<TrialThenNumber...>::Trials_t>;
  constexpr static Numbers_t Numbers = Numbers_t();
};
template<typename TTrial, typename TNumber>
struct TrialNumberSplit<TTrial, TNumber> {
  using Numbers_t = InfoArray<TNumber>;
  using Trials_t = TrialArray<TTrial>;
  constexpr static Numbers_t Numbers = Numbers_t();  // 必须显式初始化不然不过编译
};
template<UID TUID, bool TRandom, typename... TrialThenNumber>
struct Session : public ISession {
  using TNS = TrialNumberSplit<TrialThenNumber...>;
  constexpr static uint8_t NumDistinctTrials = std::extent_v<decltype(TNS::Numbers.Array)>;
  static void ArrangeTrials(const uint16_t* TrialsLeft) {
    TrialQueue.resize(std::accumulate(TrialsLeft, TrialsLeft + NumDistinctTrials, uint16_t(0)));
    const ITrial** TQEnd = TrialQueue.data();
    for (uint8_t T = 0; T < NumDistinctTrials; ++T) {
      TNS::Trials_t::Interfaces[T]->Setup();
      std::fill_n(TQEnd, TrialsLeft[T], TNS::Trials_t::Interfaces[T]);
      TQEnd += TrialsLeft[T];
    }
    if (TRandom)
      std::shuffle(TrialQueue.data(), TQEnd, Urng);
    TrialsDone = 0;
  }

public:
  constexpr Session()
    : ISession(TUID) {
  }
  void WriteInfo() const override {
    constexpr auto Info = InfoStruct(Info_UID, TUID, Info_Random, TRandom, Info_DistinctTrials, TNS::Trials_t::Info, Info_NumTrials, TNS::Numbers);
    SerialWrite(Info);
  }
  void Start() const override {
    ArrangeTrials(TNS::Numbers.Array);
    SerialWrite<uint16_t>(TrialQueue.size());
    RunAsync();
  }
  void Restore(uint8_t NDT, const RestoreInfo* RIs) const override {
    const std::unique_ptr<uint16_t[]> NumTrialsLeft = std::make_unique<uint16_t[]>(NumDistinctTrials);
    const RestoreInfo* const RIEnd = RIs + NDT;
    TrialsRestored = 0;
    for (uint8_t T = 0; T < NumDistinctTrials; ++T) {
      uint16_t NT = TNS::Numbers.Array[T];
      for (const RestoreInfo* RIBegin = RIs; RIBegin < RIEnd; ++RIBegin)
        if (TNS::Trials_t::Interfaces[T]->MyUID == RIBegin->TrialUID) {
          NT -= RIBegin->NumDone;
          TrialsRestored += RIBegin->NumDone;
          break;
        }
      NumTrialsLeft[T] = NT;
    }
    ArrangeTrials(NumTrialsLeft.get());
    RunAsync();
  }
};
template<typename... Ts>
const std::map<UID, const ISession*> SessionMap_t{ { Instance<Ts>.MyUID, &Instance<Ts> }... };