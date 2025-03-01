#pragma once
//生命周期管理策略：每个会话或测试开始时统一构造所有组件，结束时统一析构所有组件，中途不进行构造和析构。全部静态占内存太多，全部动态性能太低，因此采用此权衡策略。
#include "ISession.h"
#include "SerialIO.h"
#define TOFA_TIMER0
#define TOFA_TIMER1
#define TOFA_TIMER2
#define TOFA_TIMER3
#define TOFA_TIMER4
#define TOFA_TIMER5
#include <TimersOneForAll_Declare.hpp>
#include <Quick_digital_IO_interrupt.hpp>
#include <algorithm>
#include <numeric>
#include <set>
#include <cmath>
#include <unordered_map>
#include <string>
extern UID State;
template <uint8_t Pin>
struct PinStates
{
  static std::set<std::move_only_function<void() const> const *> CallbackSet;
  static Timers_one_for_all::TimerClass const *FlashingTimer;
  static void TraverseCallback()
  {
    if (State != State_SessionPaused)
      for (std::move_only_function<void() const> const *const C : CallbackSet)
        (*C)();
  }
  static std::move_only_function<void() const> ReportHit;
};
template <uint8_t Pin>
std::set<std::move_only_function<void() const> const *> PinStates<Pin>::CallbackSet;
template <uint8_t Pin>
Timers_one_for_all::TimerClass const *PinStates<Pin>::FlashingTimer;
template <uint8_t Pin>
std::move_only_function<void() const> PinStates<Pin>::ReportHit;

// 如果试图添加重复的中断回调，则不会添加，也不会出错
template <uint8_t Pin>
void RisingInterrupt(std::move_only_function<void() const> const *Callback)
{
  if (PinStates<Pin>::CallbackSet.empty())
  {
    // 清除attachInterrupt之前已经立起并被记住的中断旗帜。中断旗帜与引脚号的对应关系与digitalPinToInterrupt不同。
#ifdef ARDUINO_ARCH_AVR
    Quick_digital_IO_interrupt::ClearInterrupt<Pin>();
#elif defined(ARDUINO_ARCH_SAM)
    Quick_digital_IO_interrupt::ClearInterruptPending<Pin>();
#else
#error 未知的芯片类型
#endif
    Quick_digital_IO_interrupt::AttachInterrupt<Pin, RISING>(PinStates<Pin>::TraverseCallback);
  }
  PinStates<Pin>::CallbackSet.insert(Callback);
}

template <uint8_t Pin>
void DetachInterrupt(std::move_only_function<void() const> const *Callback)
{
  PinStates<Pin>::CallbackSet.erase(Callback);
  if (PinStates<Pin>::CallbackSet.empty())
    // detachInterrupt不能阻止后续中断旗帜再被立起
    Quick_digital_IO_interrupt::DetachInterrupt<Pin>();
}
struct ITest
{
  UID MyUID;
  // 测试开始时将调用此方法。测试分为自动结束型和手动结束型。对于自动结束型，应当根据TestTimes参数将测试重复指定的次数，并返回true表示该测试将自动结束。对于手动结束型，一般应当忽略TestTimes参数，返回false，持续测试直到Stop被调用。
  virtual bool Start(uint16_t TestTimes) = 0;
  // 测试被用户手动结束时将调用此方法。自动结束型测试无需实现此方法，手动结束型则必须实现。
  virtual void Stop() const {}
  constexpr ITest(UID MyUID)
      : MyUID(MyUID)
  {
  }
};
// 给引脚一段时间的高或低电平，然后反转
template <UID TMyUID, uint8_t Pin, uint16_t Milliseconds, bool HighOrLow = HIGH>
struct PinFlashTest : public ITest
{
  constexpr PinFlashTest()
      : ITest(TMyUID)
  {
  }
  bool Start(uint16_t TestTimes) override
  {
    if (PinStates<Pin>::NeedSetup)
    {
      Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
      PinStates<Pin>::NeedSetup = false;
    }
    Quick_digital_IO_interrupt::DigitalWrite<Pin, HighOrLow>();
    Timers_one_for_all::TimerClass const *const Timer = Timers_one_for_all::AllocateTimer();
    constexpr std::chrono::milliseconds Delay{Milliseconds};
    Timer->DoAfter(Delay * TestTimes, [Timer = Timer]()
                   {
      Timer->Allocatable(true);
      Quick_digital_IO_interrupt::DigitalToggle<Pin>(); });
    return true;
  }
};
template <uint16_t Min, uint16_t Max>
uint16_t LogRandom()
{
  constexpr float LogMin = std::log2(Min);
  constexpr uint16_t Range = Max - Min;
  return pow(2, random(Range) * (std::log2(Max) - LogMin) / Range + LogMin);
}
template <uint16_t HighMilliseconds, uint16_t LowMilliseconds, uint16_t RandomCycleMin, uint16_t RandomCycleMax>
void PopulateRandomCycles(std::vector<std::chrono::milliseconds> &FlashCycles, uint8_t Times = 1)
{
  constexpr uint16_t FullMilliseconds = HighMilliseconds + LowMilliseconds;
  constexpr float HighRatio = static_cast<float>(HighMilliseconds) / FullMilliseconds;
  FlashCycles.clear();
  uint16_t MillisecondsLeft = FullMilliseconds * Times;
  while (MillisecondsLeft)
  {
    uint16_t RandomCycle = LogRandom<RandomCycleMin, RandomCycleMax>();
    if (RandomCycle + RandomCycleMin > MillisecondsLeft)
      RandomCycle = RandomCycle < MillisecondsLeft ? (RandomCycleMax < MillisecondsLeft ? MillisecondsLeft - RandomCycleMin : (RandomCycle < MillisecondsLeft + MillisecondsLeft - RandomCycleMin ? MillisecondsLeft - RandomCycleMin : MillisecondsLeft)) : MillisecondsLeft;
    MillisecondsLeft -= RandomCycle;
    const float FloatHigh = RandomCycle * HighRatio;
    uint16_t IntegerHigh = FloatHigh;
    if (IntegerHigh & 1)
    {
      if (FloatHigh >= IntegerHigh + 0.5)
        IntegerHigh++;
    }
    else
    {
      if (FloatHigh > IntegerHigh + 0.5)
        IntegerHigh++;
    }
    FlashCycles.emplace_back(IntegerHigh);
    FlashCycles.emplace_back(RandomCycle - IntegerHigh);
  }
}
template <UID TUID>
struct OfUID
{
  static constexpr UID MyUID = TUID;
};
// 给引脚一段时间的高或低电平，然后反转
template <UID TMyUID, uint8_t Pin, uint16_t HighMilliseconds, uint16_t LowMilliseconds, uint16_t RandomCycleMin, uint16_t RandomCycleMax>
struct RandomFlashTest : ITest, OfUID<TMyUID>
{
  constexpr RandomFlashTest()
      : ITest(TMyUID)
  {
  }
  Timers_one_for_all::TimerClass const *Timer;
  std::vector<std::chrono::milliseconds> FlashCycles;
  std::vector<std::chrono::milliseconds>::const_iterator NextCycle;
  bool Start(uint16_t TestTimes) override
  {
    Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
    if (HighMilliseconds)
    {
      PopulateRandomCycles<HighMilliseconds, LowMilliseconds, RandomCycleMin, RandomCycleMax>(FlashCycles);
      NextCycle = FlashCycles.cbegin();
      Timer = Timers_one_for_all::AllocateTimer();
      SetHigh();
    }
    return true;
  }

protected:
  void SetHigh()
  {
    if (NextCycle < FlashCycles.cend())
    {
      Quick_digital_IO_interrupt::DigitalWrite<Pin, HIGH>();
      Timer->DoAfter(*(NextCycle++), [this]()
                     { this->SetLow(); });
    }
    else
      Timer->Allocatable(true);
  }
  void SetLow()
  {
    Quick_digital_IO_interrupt::DigitalWrite<Pin, LOW>();
    Timer->DoAfter(*(NextCycle++), [this]()
                   { this->SetHigh(); });
  }
};
// 监视引脚，每次高电平发送串口报告。此测试需要调用Stop才能终止，且无视TestTimes参数
inline static std::move_only_function<void() const> const MonitorTest_ReportHit{[]()
                                                                                {
                                                                                  SerialWrite(Signal_MonitorHit);
                                                                                }};
template <UID TMyUID, uint8_t Pin>
struct MonitorTest : ITest, OfUID<TMyUID>
{
  constexpr MonitorTest()
      : ITest(TMyUID)
  {
  }
  bool Start(uint16_t) override
  {
    static_assert(Quick_digital_IO_interrupt::PinInterruptable(Pin), "试图将不支持的引脚用于MonitorTest");
    if (PinStates<Pin>::NeedSetup)
    {
      Quick_digital_IO_interrupt::PinMode<Pin, INPUT>();
      PinStates<Pin>::NeedSetup = false;
    }
    RisingInterrupt<Pin>(&MonitorTest_ReportHit);
    return false;
  }
  void Stop() const override
  {
    DetachInterrupt<Pin>(&MonitorTest_ReportHit);
  }
};
// 测试具有指定高电平和低电平毫秒数和循环次数的方波
template <UID TMyUID, uint8_t Pin, uint16_t HighMilliseconds, uint16_t LowMilliseconds, uint16_t NumCycles>
struct SquareWaveTest : ITest, OfUID<TMyUID>
{
  constexpr SquareWaveTest()
      : ITest(TMyUID)
  {
  }
  Timers_one_for_all::TimerClass const *Timer;
  bool Start(uint16_t TestTimes) override
  {
    if (PinStates<Pin>::NeedSetup)
    {
      Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
      PinStates<Pin>::NeedSetup = false;
    }
    Timer = Timers_one_for_all::AllocateTimer();
    Timer->DoubleRepeat(std::chrono::milliseconds{LowMilliseconds}, Quick_digital_IO_interrupt::DigitalWrite<Pin, HIGH>, std::chrono::milliseconds{HighMilliseconds}, Quick_digital_IO_interrupt::DigitalWrite<Pin, LOW>, NumCycles * TestTimes * 2, [Timer = Timer]()
                        { Timer->Allocatable(true); });
    return true;
  }
};
// 测试具有指定高电平和低电平毫秒数和循环次数的方波
template <UID TMyUID, uint8_t Pin, uint32_t FrequencyHz, uint16_t Milliseconds>
struct ToneTest : ITest, OfUID<TMyUID>
{
  constexpr ToneTest()
      : ITest(TMyUID)
  {
  }
  Timers_one_for_all::TimerClass const *Timer;
  bool Start(uint16_t TestTimes) override
  {
    Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
    Timer = Timers_one_for_all::AllocateTimer();
    Timer->RepeatEvery(std::chrono::microseconds{500000 / FrequencyHz}, Quick_digital_IO_interrupt::DigitalToggle<Pin>, std::chrono::microseconds{Milliseconds * TestTimes * 1000}, [Timer = Timer]()
                       { Timer->Allocatable(true); });
    return true;
  }
};
template <typename... Ts>
struct UidMap
{
  static const std::unordered_map<UID, ITest *(*)()> Tests;
  static const std::unordered_map<UID, ISession *(*)()> Sessions;
};
template <typename... Ts>
const std::unordered_map<UID, ITest *(*)()> UidMap<Ts...>::Tests{{Ts::MyUID, []()
                                                                  { return new Ts; }}...};
template <typename... Ts>
const std::unordered_map<UID, ISession *(*)()> UidMap<Ts...>::Sessions{{Ts::MyUID, []()
                                                                        { return new Ts; }}...};
template <typename ToAdd, typename Container>
struct _AddToArray;
template <typename ToAdd, template <typename...> class Container, typename... AlreadyIn>
struct _AddToArray<ToAdd, Container<AlreadyIn...>>
{
  using Result = Container<ToAdd, AlreadyIn...>;
};
template <typename ToAdd, typename Container>
using AddToArray = typename _AddToArray<ToAdd, Container>::Result;
template <typename T>
struct TypeToUID
{
  static constexpr UID value = T::MyUID;
};
template <>
struct TypeToUID<bool>
{
  static constexpr UID value = Type_Bool;
};
template <>
struct TypeToUID<uint8_t>
{
  static constexpr UID value = Type_UInt8;
};
template <>
struct TypeToUID<uint16_t>
{
  static constexpr UID value = Type_UInt16;
};
template <>
struct TypeToUID<UID>
{
  static constexpr UID value = Type_UID;
};
#pragma pack(push, 1)
template <typename T, typename... Ts>
struct InfoArray
{
  static constexpr UID MyUID = Type_Array;
  uint8_t Number = (sizeof...(Ts) + 1); // SAM架构的bug，必须套一层括号
  UID Type = TypeToUID<typename T::value_type>::value;
  typename T::value_type Array[sizeof...(Ts) + 1] = {T::value, Ts::value...};
};
template <typename T, typename... Ts>
struct CellArray
{
  UID Type = TypeToUID<T>::value;
  T Value;
  CellArray<Ts...> Values;
  constexpr CellArray(T Value, Ts... Values)
      : Value(Value), Values(CellArray<Ts...>(Values...))
  {
  }
};
template <typename T>
struct CellArray<T>
{
  UID Type = TypeToUID<T>::value;
  T Value;
  constexpr CellArray(T Value)
      : Value(Value)
  {
  }
};
template <typename... T>
struct _InfoCell
{
  static constexpr UID MyUID = Type_Cell;
  uint8_t NumCells = 0;
};
template <typename T, typename... Ts>
struct _InfoCell<T, Ts...>
{
  static constexpr UID MyUID = Type_Cell;
  uint8_t NumCells = (sizeof...(Ts) + 1);
  CellArray<T, Ts...> Values;
  constexpr _InfoCell(T Value, Ts... Values)
      : Values(CellArray<T, Ts...>(Value, Values...))
  {
  }
};
template <typename TName, typename TValue, typename... Ts>
struct InfoFields
{
  TName Name;
  UID Type = TypeToUID<TValue>::value;
  TValue Value;
  InfoFields<Ts...> FollowingFields;
  constexpr InfoFields(TName Name, TValue Value, Ts... NameValues)
      : Name(Name), Value(Value), FollowingFields(InfoFields<Ts...>(NameValues...))
  {
  }
};
template <typename TName, typename TValue>
struct InfoFields<TName, TValue>
{
  TName Name;
  UID Type = TypeToUID<TValue>::value;
  TValue Value;
  constexpr InfoFields(TName Name, TValue Value)
      : Name(Name), Value(Value)
  {
  }
};
template <typename... Ts>
struct _InfoStruct
{
  static constexpr UID MyUID = Type_Struct;
  uint8_t NumFields = 0;
};
template <typename T, typename... Ts>
struct _InfoStruct<UID, T, Ts...>
{
  static constexpr UID MyUID = Type_Struct;
  uint8_t NumFields = (sizeof...(Ts) / 2 + 1);
  InfoFields<UID, T, Ts...> Fields;
  constexpr _InfoStruct(UID Name, T Value, Ts... NameValues)
      : Fields(InfoFields<UID, T, Ts...>(Name, Value, NameValues...))
  {
  }
};
#pragma pack(pop)
#ifdef __cpp_deduction_guides
#define InfoCell _InfoCell
#define InfoStruct _InfoStruct
// 只有主模板能推断，特化模板必须加推断向导
template <typename T, typename... Ts>
InfoCell(T, Ts...) -> InfoCell<T, Ts...>;
template <typename... Ts>
InfoStruct(UID, Ts...) -> InfoStruct<UID, Ts...>;
#else
template <typename... T>
constexpr _InfoStruct<T...> InfoStruct(T... NameValues)
{
  return _InfoStruct<T...>(NameValues...);
}
template <typename... T>
constexpr _InfoCell<T...> InfoCell(T... Values)
{
  return _InfoCell<T...>(Values...);
}
#endif
using NullStep = IStep;
// 持续等待，直到指定引脚在指定连续毫秒数内都保持静默才结束
template <uint8_t Pin, uint16_t MinMilliseconds, uint16_t MaxMilliseconds = MinMilliseconds, UID MyUID = Step_Calmdown>
class CalmdownStep : public IStep
{
  static constexpr bool RandomTime = MinMilliseconds < MaxMilliseconds;
  Timers_one_for_all::TimerClass const *Timer;
  std::move_only_function<void() const> FinishCallback;
  std::chrono::milliseconds CurrentDuration = std::chrono::milliseconds(MinMilliseconds); // SAM的bug，此处无法直接用花括号初始化
  std::move_only_function<void() const> const Reset;
  void TimeUp()
  {
    DetachInterrupt<Pin>(&Reset);
    Timer->Allocatable(true);
    FinishCallback();
  };

public:
  bool Start(std::move_only_function<void() const> &&FC) override
  {
    FinishCallback = std::move(FC);
    if (RandomTime)
      CurrentDuration = std::chrono::milliseconds{LogRandom<MinMilliseconds, MaxMilliseconds>()};
    Timer = Timers_one_for_all::AllocateTimer();
    Timer->DoAfter(CurrentDuration, [this]()
                   { TimeUp(); 
                  delete this;});
    RisingInterrupt<Pin>(&Reset);
    return true;
  }
  void Pause() const override
  {
    DetachInterrupt<Pin>(&Reset);
    Timer->Pause();
  }
  void Continue() const override
  {
    RisingInterrupt<Pin>(&Reset);
    Timer->Continue();
  }
  void Abort() const override
  {
    DetachInterrupt<Pin>(&Reset);
    Timer->Stop();
    Timer->Allocatable(true);
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_MinMilliseconds, MinMilliseconds, Info_MaxMilliseconds, MaxMilliseconds);
  CalmdownStep()
      : Reset{[this]()
              {
                Timer->DoAfter(CurrentDuration, [this]()
                               { TimeUp(); });
              }}
  {
    static_assert(Quick_digital_IO_interrupt::PinInterruptable(Pin), "试图将不支持中断的引脚用于CalmdownStep");
    Quick_digital_IO_interrupt::PinMode<Pin, INPUT>();
  }
};
// 让引脚高电平一段时间再回到低电平。异步执行，不阻塞时相
template <uint8_t Pin, uint16_t Milliseconds, typename UpReporter = NullStep, typename DownReporter = NullStep, UID MyUID = Step_PinFlash>
class PinFlashStep : public IStep
{
  void DownReport()
  {
    Quick_digital_IO_interrupt::DigitalWrite<Pin, LOW>();
    Report<DownReporter>();
    Timer->Allocatable(true);
  }
  Timers_one_for_all::TimerClass const *Timer;

public:
  bool Start(std::move_only_function<void() const> &&) override
  {
    UpReporter().Start();
    Quick_digital_IO_interrupt::DigitalWrite<Pin, HIGH>();
    Timer = Timers_one_for_all::AllocateTimer();
    Timer->DoAfter(std::chrono::milliseconds{Milliseconds}, [this]()
                   { 
                    DownReport(); 
                  delete this; });
    return false;
  }
  PinFlashStep()
  {
    Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_Milliseconds, Milliseconds);
};
// 在一段时间内随机闪烁引脚。指定高电平和低电平总时长，以及每次闪烁的随机范围。
template <uint8_t Pin, uint16_t HighMilliseconds, uint16_t LowMilliseconds, uint16_t RandomCycleMin, uint16_t RandomCycleMax, typename UpReporter = NullStep, typename DownReporter = NullStep, bool ReportEachCycle = false, UID MyUID = Step_RandomFlash>
class RandomFlashStep : public IStep
{
  std::vector<std::chrono::milliseconds> FlashCycles;
  Timers_one_for_all::TimerClass const *Timer;
  std::vector<std::chrono::milliseconds>::const_iterator CurrentCycle;
  void SetHigh()
  {
    if (CurrentCycle < FlashCycles.cend())
    {
      if (ReportEachCycle)
        Report<UpReporter>();
      Quick_digital_IO_interrupt::DigitalWrite<Pin, HIGH>();
      Timer->DoAfter(*(CurrentCycle++), [this]()
                     { this->SetLow(); });
    }
    else
      Timer->Allocatable(true);
  }
  void SetLow()
  {
    if (ReportEachCycle)
      Report<DownReporter>();
    Quick_digital_IO_interrupt::DigitalWrite<Pin, LOW>();
    Timer->DoAfter(*(CurrentCycle++), [this]()
                   { this->SetHigh(); });
  }

public:
  bool Start(std::move_only_function<void() const> &&) override
  {
    if (!ReportEachCycle)
      Report<UpReporter>();
    if (HighMilliseconds)
    {
      PopulateRandomCycles<HighMilliseconds, LowMilliseconds, RandomCycleMin, RandomCycleMax>(FlashCycles);
      CurrentCycle = FlashCycles.cbegin();
      Timer = Timers_one_for_all::AllocateTimer();
      SetHigh();
    }
    else if (!ReportEachCycle)
      Report<DownReporter>();
    return false;
  }
  void Setup() override
  {
    if (PinStates<Pin>::NeedSetup)
    {
      Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
      PinStates<Pin>::NeedSetup = false;
    }
    Instance<UpReporter>::value.Setup();
    Instance<DownReporter>::value.Setup();
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_HighMilliseconds, HighMilliseconds, Info_LowMilliseconds, LowMilliseconds, Info_RandomCycleMin, RandomCycleMin, Info_RandomCycleMax, RandomCycleMax);
};
// 后台无限随机闪烁引脚，直到StopRandomFlash。指定每个周期的高电平和低电平时长范围。
template <uint8_t Pin, uint16_t MinHighMilliseconds, uint16_t MaxHighMilliseconds, uint16_t MinLowMilliseconds, uint16_t MaxLowMilliseconds, UID MyUID = Step_StartRandomFlash>
class StartRandomFlash : public IStep
{
  static void SetHigh()
  {
    Quick_digital_IO_interrupt::DigitalWrite<Pin, HIGH>();
    PinStates<Pin>::FlashingTimer->DoAfter(std::chrono::milliseconds{LogRandom<MinHighMilliseconds, MaxHighMilliseconds>()}, SetLow);
  }
  static void SetLow()
  {
    Quick_digital_IO_interrupt::DigitalWrite<Pin, LOW>();
    PinStates<Pin>::FlashingTimer->DoAfter(std::chrono::milliseconds{LogRandom<MinLowMilliseconds, MaxLowMilliseconds>()}, SetHigh);
  }

public:
  bool Start(std::move_only_function<void() const> &&) override
  {
    PinStates<Pin>::FlashingTimer = Timers_one_for_all::AllocateTimer();
    SetHigh();
    return false;
  }
  void Setup() override
  {
    if (PinStates<Pin>::NeedSetup)
    {
      Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
      PinStates<Pin>::NeedSetup = false;
    }
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_MinHighMilliseconds, MinHighMilliseconds, Info_MaxHighMilliseconds, MaxHighMilliseconds, Info_MinLowMilliseconds, MinLowMilliseconds, Info_MaxLowMilliseconds, MaxLowMilliseconds);
};
// 停止后台无限随机闪烁引脚
template <uint8_t Pin, UID MyUID = Step_StopRandomFlash>
class StopRandomFlash : public IStep
{
public:
  bool Start(std::move_only_function<void() const> &&) override
  {
    PinStates<Pin>::FlashingTimer->Stop();
    Quick_digital_IO_interrupt::DigitalWrite<Pin, LOW>();
    PinStates<Pin>::FlashingTimer->Allocatable(true);
    return false;
  }
  void Setup() override
  {
    if (PinStates<Pin>::NeedSetup)
    {
      Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
      PinStates<Pin>::NeedSetup = false;
    }
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin);
};

// 在一段时间内同步监视引脚，发现高电平立即汇报。HitReporter和MissReporter都是IStep类型，一律异步执行，不等待
template <uint8_t Pin, uint16_t Milliseconds, uint8_t Flags, typename HitReporter, typename MissReporter = NullStep, UID MyUID = Step_Monitor>
class MonitorStep : public IStep
{
  static constexpr bool ReportOnce = Flags & Monitor_ReportOnce;
  static constexpr bool ReportMiss = !std::is_same_v<MissReporter, NullStep> _CSL_Parentheses11;
  static constexpr bool HitAndFinish = Flags & Monitor_HitAndFinish;
  std::move_only_function<void() const> FinishCallback;
  bool NoHits;
  Timers_one_for_all::TimerClass const *Timer;
  std::move_only_function<void() const> const HitReport;
  void TimeUp()
  {
    DetachInterrupt<Pin>(&HitReport);
    if (ReportMiss && NoHits)
      Report<MissReporter>();
    Timer->Allocatable(true);
    FinishCallback();
  }

public:
  bool Start(std::move_only_function<void() const> &&FC) override
  {
    FinishCallback = std::move(FC);
    if (ReportMiss)
      NoHits = true;
    RisingInterrupt<Pin>(&HitReport);
    Timer = Timers_one_for_all::AllocateTimer();
    Timer->DoAfter(std::chrono::milliseconds{Milliseconds}, [this]()
                   { TimeUp(); });
    return true;
  }
  void Pause() const override
  {
    DetachInterrupt<Pin>(&HitReport);
    Timer->Pause();
  }
  void Continue() const override
  {
    Timer->Continue();
    RisingInterrupt<Pin>(&HitReport);
  }
  void Abort() const override
  {
    DetachInterrupt<Pin>(&HitReport);
    Timer->Stop();
    Timer->Allocatable(true);
  }
  void Setup() override
  {
    if (PinStates<Pin>::NeedSetup)
    {
      static_assert(Quick_digital_IO_interrupt::PinInterruptable(Pin), "试图将不支持中断的引脚用于MonitorStep");
      Quick_digital_IO_interrupt::PinMode<Pin, INPUT>();
      PinStates<Pin>::NeedSetup = false;
    }
    Instance<HitReporter>::value.Setup();
    Instance<MissReporter>::value.Setup();
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
  MonitorStep()
      : HitReport{
            [this]()
            {
              Report<HitReporter>();
              if (HitAndFinish)
              {
                DetachInterrupt<Pin>(&HitReport);
                Timer->Stop();
                Timer->Allocatable(true);
                FinishCallback();
              }
              else
              {
                if (ReportOnce)
                  DetachInterrupt<Pin>(&HitReport);
                if (ReportMiss)
                  NoHits = false;
              }
            }} {}
};
// 不做任何事，同步等待一段时间后再进入下一步。
template <uint16_t MinMilliseconds, uint16_t MaxMilliseconds = MinMilliseconds, UID MyUID = Step_Wait>
struct WaitStep : public IStep
{
  Timers_one_for_all::TimerClass const *Timer;
  bool Start(std::move_only_function<void() const> &&FinishCallback) override
  {
    Timer = Timers_one_for_all::AllocateTimer();
    std::move_only_function<void() const> FC = [Timer = Timer, FinishCallback = std::move(FinishCallback)]()
    {
      Timer->Allocatable(true);
      FinishCallback();
    };
    if (MinMilliseconds < MaxMilliseconds)
      Timer->DoAfter(std::chrono::milliseconds{LogRandom<MinMilliseconds, MaxMilliseconds>()}, std::move(FC));
    else
      Timer->DoAfter(std::chrono::milliseconds{MinMilliseconds}, std::move(FC));
    return true;
  }
  void Pause() const override
  {
    Timer->Pause();
  }
  void Continue() const override
  {
    Timer->Continue();
  }
  void Abort() const override
  {
    Timer->Stop();
    Timer->Allocatable(true);
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_MinMilliseconds, MinMilliseconds, Info_MaxMilliseconds, MaxMilliseconds);
};
// 开始监视指定引脚，发现高电平立即汇报。异步执行，步骤本身立即结束，但会在后台持续监视，直到StopMonitorStep才会结束监视。
template <uint8_t Pin, typename Reporter, UID MyUID = Step_StartMonitor>
struct StartMonitorStep : public IStep
{
  bool Start(std::move_only_function<void() const> &&) override
  {
    PinStates<Pin>::ReportHit = Report<Reporter>;
    RisingInterrupt<Pin>(&PinStates<Pin>::ReportHit);
    return false;
  }
  void Setup() override
  {
    if (PinStates<Pin>::NeedSetup)
    {
      static_assert(Quick_digital_IO_interrupt::PinInterruptable(Pin), "试图将不支持中断的引脚用于StartMonitorStep");
      Quick_digital_IO_interrupt::PinMode<Pin, INPUT>();
      PinStates<Pin>::NeedSetup = false;
    }
    Instance<Reporter>::value.Setup();
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_Reporter, Reporter::Info);
};
// 停止监视指定引脚。如果一个引脚被多个Reporter监视，此步骤只会终止指定Reporter的监视，其它Reporter照常工作。
template <uint8_t Pin, typename Reporter, UID MyUID = Step_StopMonitor>
struct StopMonitorStep : public IStep
{
  bool Start(std::move_only_function<void() const> &&) override
  {
    DetachInterrupt<Pin>(&PinStates<Pin>::ReportHit);
    return false;
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_Reporter, Reporter::Info);
  // 此类不需要Setup，初始化应由与其配对的StartMonitorStep负责
};
// 向串口写出一个字节
template <UID ToWrite, UID MyUID = Step_SerialWrite>
struct SerialWriteStep : public IStep
{
  bool Start(std::move_only_function<void() const> &&) override
  {
    SerialWrite(ToWrite);
    return false;
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_ToWrite, ToWrite);
};
// 以精确的毫秒数记录事件。注意，如果中途发生断线重连事件，事件前后的相对时间将不再精确。
extern uint32_t TimeShift;
template <UID Event, UID MyUID = Step_PreciseLog>
struct PreciseLogStep : public IStep
{
  Timers_one_for_all::TimerClass const *Timer = nullptr;
  void Setup() override
  {
    // 必须每次Setup，否则无法重复使用。没有办法还原“已Setup”标志位。
    if (!Timer)
      Timer = Timers_one_for_all::AllocateTimer();
    Timer->StartTiming();
    // 一旦Setup，此计时器将被永久占用，不会释放
  }
  bool Start(std::move_only_function<void() const> &&) override
  {
    SerialWrite(Signal_PreciseLog);
    SerialWrite(Event);
    SerialWrite(Timer->GetTiming<std::chrono::milliseconds>().count() + TimeShift);
    return false;
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Event, Event);
};
// 播放具有指定频率㎐和指定毫秒数的声音。异步执行，步骤不阻塞时相。
template <uint8_t Pin, uint16_t FrequencyHz, uint16_t Milliseconds, typename UpReporter = NullStep, typename DownReporter = NullStep, UID MyUID = Step_Audio>
struct ToneStep : public IStep
{
  Timers_one_for_all::TimerClass const *Timer;
  bool Start(std::move_only_function<void() const> &&) override
  {
    Report<UpReporter>();
    Timer = Timers_one_for_all::AllocateTimer();
    Timer->RepeatEvery(std::chrono::microseconds{500000 / FrequencyHz}, Quick_digital_IO_interrupt::DigitalToggle<Pin>, std::chrono::microseconds{Milliseconds * 1000}, [Timer = Timer]()
                       {
      Timer->Allocatable(true);
      Report<DownReporter>(); });
    return false;
  }
  void Setup() override
  {
    if (PinStates<Pin>::NeedSetup)
    {
      Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
      PinStates<Pin>::NeedSetup = false;
    }
    Instance<UpReporter>::value.Setup();
    Instance<DownReporter>::value.Setup();
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_FrequencyHz, FrequencyHz, Info_Milliseconds, Milliseconds);
};
// 播放具有指定高电平和低电平毫秒数的方波。异步执行，步骤不阻塞时相。
template <uint8_t Pin, uint16_t HighMilliseconds, uint16_t LowMilliseconds, uint16_t NumCycles, typename UpReporter = NullStep, typename DownReporter = NullStep, UID MyUID = Step_SquareWave>
struct SquareWaveStep : public IStep
{
  Timers_one_for_all::TimerClass const *Timer;
  bool Start(std::move_only_function<void() const> &&) override
  {
    Report<UpReporter>();
    Timer = Timers_one_for_all::AllocateTimer();
    Timer->DoubleRepeat(std::chrono::milliseconds{LowMilliseconds}, Quick_digital_IO_interrupt::DigitalWrite<Pin, HIGH>, std::chrono::milliseconds{HighMilliseconds}, Quick_digital_IO_interrupt::DigitalToggle<Pin, LOW>, NumCycles * 2, [Timer = Timer]()
                        {
      Timer->Allocatable(true);
      Report<DownReporter>(); });
    return false;
  }
  void Setup() override
  {
    if (PinStates<Pin>::NeedSetup)
    {
      Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
      PinStates<Pin>::NeedSetup = false;
    }
    Instance<UpReporter>::value.Setup();
    Instance<DownReporter>::value.Setup();
  }
  static constexpr auto Info = InfoStruct(Info_UID, MyUID, Info_Pin, Pin, Info_HighMilliseconds, HighMilliseconds, Info_LowMilliseconds, LowMilliseconds, Info_NumCycles, NumCycles);
};
template <typename... SubSteps>
struct IndividualThreadStep : IStep
{
  static constexpr IStep *StepPointers[] = {&Instance<SubSteps>::value...};
  static IStep *const *CurrentStep;
  void Setup() override
  {
    for (IStep *S : StepPointers)
      S->Setup();
  }
  static void ContinueCycle()
  {
    while (CurrentStep < std::end(StepPointers) && !(*CurrentStep++)->Start(ContinueCycle))
      ;
  }
  bool Start(std::move_only_function<void() const> &&) override
  {
    for (CurrentStep = StepPointers; CurrentStep < std::end(StepPointers) && !(*CurrentStep++)->Start(ContinueCycle);)
      ;
    return false;
  }
  static constexpr auto Info = InfoStruct(Info_UID, Step_IndividualThread, Info_SubSteps, InfoCell(SubSteps::Info...));
};
template <typename... SubSteps>
IStep *const *IndividualThreadStep<SubSteps...>::CurrentStep;
template <UID TUID, typename... TSteps>
class Trial : public ITrial
{
  static IStep *Steps[sizeof...(TSteps)];
  static void NextStep()
  {
    while (StepsDone < sizeof...(TSteps))
      if (Steps[StepsDone++]->Start(NextStep))
        return;
    FinishCallback();
  }

public:
  static constexpr auto Info = InfoStruct(Info_UID, TUID, Info_Steps, InfoCell(TSteps::Info...));
  constexpr Trial()
      : ITrial(TUID)
  {
  }
  void Setup() override
  {
    for (IStep *S : Steps)
      S->Setup();
  }
  bool Start(std::move_only_function<void() const> &&FC) override
  {
    FinishCallback = std::move(FC);
    StepsDone = 0;
    while (StepsDone < sizeof...(TSteps))
      if (Steps[StepsDone++]->Start(NextStep))
        return true;
    return false;
  }
  void Pause() const override
  {
    Steps[StepsDone - 1]->Pause();
  }
  void Continue() const override
  {
    Steps[StepsDone - 1]->Continue();
  }
  void Abort() const override
  {
    Steps[StepsDone - 1]->Abort();
  }
};
template <UID TUID, typename... TSteps>
IStep *Trial<TUID, TSteps...>::Steps[sizeof...(TSteps)] = {&Instance<TSteps>::value...};
template <uint16_t Value>
using N = std::integral_constant<uint16_t, Value>;
template <typename... Trials>
struct TrialArray
{
  static ITrial *Interfaces[sizeof...(Trials)];
  static constexpr auto Info = InfoCell(Trials::Info...);
};
// 静态成员必须类外定义，即使模板也一样
template <typename... Trials>
ITrial *TrialArray<Trials...>::Interfaces[sizeof...(Trials)] = {&Instance<Trials>::value...};
template <typename TTrial, typename TNumber, typename... TrialThenNumber>
struct TrialNumberSplit
{
  using Numbers_t = AddToArray<TNumber, typename TrialNumberSplit<TrialThenNumber...>::Numbers_t>;
  using Trials_t = AddToArray<TTrial, typename TrialNumberSplit<TrialThenNumber...>::Trials_t>;
  static constexpr Numbers_t Numbers = Numbers_t();
};
template <typename TTrial, typename TNumber>
struct TrialNumberSplit<TTrial, TNumber>
{
  using Numbers_t = InfoArray<TNumber>;
  using Trials_t = TrialArray<TTrial>;
  static constexpr Numbers_t Numbers = Numbers_t(); // 必须显式初始化不然不过编译
};
extern std::string InfoBuffer;
template <UID TUID, bool TRandom, typename... TrialThenNumber>
struct Session : public ISession
{
  using TNS = TrialNumberSplit<TrialThenNumber...>;
  constexpr static uint8_t NumDistinctTrials = std::extent_v<decltype(TNS::Numbers.Array)> _CSL_Parentheses11;
  static void ArrangeTrials(const uint16_t *TrialsLeft)
  {
    TrialQueue.resize(std::accumulate(TrialsLeft, TrialsLeft + NumDistinctTrials, uint16_t(0)));
    ITrial **TQEnd = TrialQueue.data();
    for (uint8_t T = 0; T < NumDistinctTrials; ++T)
    {
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
      : ISession(TUID)
  {
  }
  void WriteInfo() const override
  {
    SerialWrite(InfoStruct(Info_UID, TUID, Info_Random, TRandom, Info_DistinctTrials, TNS::Trials_t::Info, Info_NumTrials, TNS::Numbers));
  }
  void Start() const override
  {
    ArrangeTrials(TNS::Numbers.Array);
    SerialWrite<uint16_t>(TrialQueue.size());
    RunAsync();
  }
  void Restore(uint8_t NDT, const RestoreInfo *RIs) const override
  {
    const std::unique_ptr<uint16_t[]> NumTrialsLeft = std::make_unique<uint16_t[]>(NumDistinctTrials);
    const RestoreInfo *const RIEnd = RIs + NDT;
    TrialsRestored = 0;
    for (uint8_t T = 0; T < NumDistinctTrials; ++T)
    {
      uint16_t NT = TNS::Numbers.Array[T];
      for (const RestoreInfo *RIBegin = RIs; RIBegin < RIEnd; ++RIBegin)
        if (TNS::Trials_t::Interfaces[T]->MyUID == RIBegin->TrialUID)
        {
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