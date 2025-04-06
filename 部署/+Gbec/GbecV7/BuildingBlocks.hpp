#pragma once
#include "UID.hpp"
#include "Timers_one_for_all.hpp"
#include <Quick_digital_IO_interrupt.hpp>
#include <chrono>
#include <unordered_map>
#include <ostream>
#include <set>
#include <map>
#include <cmath>
#include "Async_stream_IO.hpp"
struct AutomaticallyEndedTest
{
	virtual void Run(uint16_t RepeatTimes) = 0;
};
// 通用步骤接口。实际步骤不一定实现所有方法。
struct Step
{
	// 返回true表示步骤已完成可以进入下一步，返回false表示步骤未完成需要等待FinishCallback。如果不重写此方法，此方法将调用Repeat然后返回true。
	virtual bool Start()
	{
		return true;
	}
	// 类似于Start，但必须重复和上次Start或Repeat完全相同的行为。这仅对于那些存在随机性的步骤有意义，对于确定性步骤此方法应当与Start做完全相同的事情。
	virtual bool Repeat()
	{
		return Start();
	}
	// 暂停当前步骤。不能暂停的步骤可以不override此方法。
	virtual void Pause() const {}
	// 继续已暂停的步骤。不能暂停的步骤可以不override此方法。
	virtual void Continue() const {}
	// 放弃当前步骤。不能放弃的步骤可以不override此方法。
	virtual void Abort() const {}
	// 从断线重连中恢复。不能恢复的步骤可以不override此方法，默认什么都不做直接返回true。
	virtual bool Restore(std::unordered_map<UID, uint16_t> &TrialsDone)
	{
		return true;
	}
	virtual ~Step() {}
	// 将步骤信息写入流中
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << static_cast<uint8_t>(UID::Type_Empty);
	}
};
using NullStep = Step;
template <typename>
struct TypeToUID;
template <>
struct TypeToUID<uint8_t>
{
	static constexpr UID Value = UID::Type_UInt8;
};
#define WriteStructSize(Size) static_cast<uint8_t>(UID::Type_Struct) << static_cast<uint8_t>(Size)
#define WriteField(Property) static_cast<uint8_t>(UID::Property_##Property) << static_cast<uint8_t>(TypeToUID<decltype(Property)>::Value) << Property
#define WriteStepID(ID) static_cast<uint8_t>(UID::Property_StepID) << static_cast<uint8_t>(UID::Type_UID) << static_cast<uint8_t>(ID)
// 向引脚写出电平
template <uint8_t Pin, bool HighOrLow>
struct DigitalWrite : Step
{
	DigitalWrite(std::move_only_function<void() const> const &)
	{
		Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
	}
	bool Start() override
	{
		Quick_digital_IO_interrupt::DigitalWrite<Pin, HighOrLow>();
		return true;
	}
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << WriteStructSize(3) << WriteStepID(UID::Step_DigitalWrite) << WriteField(Pin) << WriteField(HighOrLow);
	}
	using Repeatable = DigitalWrite<Pin, HighOrLow>;
};
// 向串口写出UID
template <UID Value>
struct SerialWrite : Step
{
	SerialWrite(std::move_only_function<void() const> const &)
	{
	}
	bool Start() override
	{
		Async_stream_IO::Send(Value, static_cast<uint8_t>(UID::Port_Signal));
		return true;
	}
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << WriteStructSize(2) << WriteStepID(UID::Step_SerialWrite) << WriteField(Value);
	}
	using Repeatable = SerialWrite<Value>;
};
template <uint16_t Count>
struct Milliseconds
{
	static constexpr std::chrono::milliseconds Get() { return std::chrono::milliseconds(Count) }
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << static_cast<uint8_t>(UID::Type_Milliseconds) << Count;
	}
};
template <uint8_t Count>
struct Seconds
{
	static constexpr std::chrono::seconds Get() { return std::chrono::milliseconds(Count) }
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << static_cast<uint8_t>(UID::Type_Seconds) << Count;
	}
};
template <typename Min, typename Max>
class RandomDuration
{
	using DurationType = decltype(Min::Get());

public:
	static DurationType Get()
	{
		constexpr float MinCount = Min::Get().count();
		return DurationType(pow(std::chrono::duration_cast<DurationType>(Min::Get()).count() / MinCount, random() / RANDOM_MAX) * MinCount);
	}
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << WriteStructSize(2) << static_cast<uint8_t>(UID::Property_Min);
		Min::WriteInfo(OutStream);
		OutStream << static_cast<uint8_t>(UID::Property_Max);
		Max::WriteInfo(OutStream);
	}
};
#define WriteDuration(FieldName, Duration) static_cast<uint8_t>(UID::Property_##FieldName) << static_cast<uint8_t>(Duration::Type) << Duration::Value.count()
// 等待一段时间，不做任何事
template <typename Duration>
struct Delay : Step
{
	Delay(std::move_only_function<void() const> const &FinishCallback) : TimerCallback([this]()
																					   {
		Timer->Allocatable = true;
		FinishCallback(); }) {}
	bool Start() override
	{
		(Timer = Timers_one_for_all::AllocateTimer())->DoAfter(Duration::Get(), TimerCallback);
		return false;
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
		Timer->Allocatable = true;
	}
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << WriteStructSize(2) << WriteStepID(UID::Step_Delay) << WriteDuration(Duration, Duration);
	}
	struct Repeatable : Delay<Duration>
	{
		using Delay<Duration>::Delay;
		bool Start() override
		{
			(Timer = Timers_one_for_all::AllocateTimer())->DoAfter(LastDuration = Duration::Get(), TimerCallback);
			return false;
		}
		bool Repeat() override
		{
			(Timer = Timers_one_for_all::AllocateTimer())->DoAfter(LastDuration, TimerCallback);
			return false;
		}

	protected:
		decltype(Duration::Get()) LastDuration;
	};

protected:
	std::move_only_function<void() const> const TimerCallback;
	Timers_one_for_all::TimerClass *Timer;
};
// 使用此容器时必须禁止中断
extern std::map<uint8_t, std::move_only_function<void() const> const *> PinMonitors;
extern std::move_only_function<void() const> const NullCallback;
#define WriteStep(FieldName)                         \
	static_cast<uint8_t>(UID::Property_##FieldName); \
	FieldName::WriteInfo(OutStream);
template <uint8_t Pin>
struct PinStates
{
	static std::move_only_function<void() const> Monitor;
};
template <uint8_t Pin>
std::move_only_function<void() const> PinStates<Pin>::Monitor;
template <typename StepType, typename = bool>
struct ContainTrials
{
	static constexpr bool value = false;
};
template <typename StepType>
struct ContainTrials<StepType, decltype(StepType::value)>
{
	static constexpr bool value = StepType::value;
};
template <UID TrialID, typename TrialStep>
struct Trial : TrialStep
{
	static constexpr bool ContainTrials = true;
	Trial(std::move_only_function<void() const> const &FinishCallback) : TrialStep(FinishCallback)
	{
		static_assert(!ContainTrials<TrialStep>::value, "Trial步骤的TrialStep不能包含Trial");
	}
	bool Start() override
	{
		Async_stream_IO::Send(TrialID, static_cast<uint8_t>(UID::Port_TrialStart));
		return TrialStep::Start();
	}
	struct Repeatable : Trial<TrialID, TrialStep::Repeatable>
	{
		using Trial<TrialID, TrialStep::Repeatable>::Trial;
		bool Repeat() override
		{
			Async_stream_IO::Send(TrialID, static_cast<uint8_t>(UID::Port_TrialStart));
			return TrialStep::Repeatable::Repeat();
		}
		using Repeatable = Repeatable;
	};
	bool Restore(std::unordered_map<UID, uint16_t> &TrialsDone) override
	{
		auto const it = TrialsDone.find(TrialID);
		if (it == TrialsDone.end())
			return Start();
		if (it->second)
		{
			if (!--it->second)
				TrialsDone.erase(it);
			return true;
		}
		else
		{
			TrialsDone.erase(it);
			return Start();
		}
	}
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << WriteStructSize(3) << WriteStepID(UID::Step_Trial) << WriteField(TrialID) << WriteStep(TrialStep);
	}
};
// 开始监视引脚，每次检测到高电平执行Reporter步骤。此步骤开始后立即结束，但监视会持续在后台执行，直到调用StopMonitor步骤。
template <uint8_t Pin, typename Reporter>
struct StartMonitor : Step
{
	StartMonitor(std::move_only_function<void() const> const &)
	{
		static_assert(!ContainTrials<Reporter>::value, "StartMonitor步骤的Reporter不能包含Trial");
		Quick_digital_IO_interrupt::PinMode<Pin, INPUT>();
		PinStates<Pin>::Monitor = [R = Reporter(NullCallback)]()
		{ R.Start(); };
	}
	bool Start() override
	{
		PinMonitors[Pin] = &PinStates<Pin>::Monitor;
		return true;
	}
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << WriteStructSize(3) << WriteStepID(UID::Step_StartMonitor) << WriteField(Pin) << WriteStep(Reporter);
	}
	using Repeatable = StartMonitor<Pin, Reporter>;
};
// 与StartMonitor相对应的步骤，停止监视引脚
template <uint8_t Pin>
struct StopMonitor : Step
{
	StopMonitor(std::move_only_function<void() const> const &) {}
	bool Start() override
	{
		PinMonitors.erase(Pin);
		return true;
	}
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << WriteStructSize(2) << WriteStepID(UID::Step_StopMonitor) << WriteField(Pin);
	}
	using Repeatable = StopMonitor<Pin>;
};
// 执行Repeatee步骤，同时监控引脚。如果Repeatee执行完之前检测到引脚电平，放弃当前执行并重启。
template <typename Repeatee, uint8_t Pin>
struct RepeatIfPin : Repeatee::Repeatable // 有些步骤的Repeatable不继承自那个步骤类型
{
	RepeatIfPin(std::move_only_function<void() const> const &ParentCallback) : ChildCallback{[ParentCallback]()
																							 {
			PinMonitors.erase(Pin);
			ParentCallback(); }},
																			   Repeatee::Repeatable(ChildCallback), MonitorCallback{[this]()
																																	{
																																		Repeatee::Repeatable::Abort();
																																		Repeatee::Repeatable::Repeat(); }}
	{
		static_assert(!ContainTrials<Repeatee::Repeatable>::value, "RepeatIfPin步骤的Repeatee不能包含Trial");
	}
	bool Start() override
	{
		if (Repeatee::Repeatable::Start())
			return true;
		PinMonitors[Pin] = &MonitorCallback;
		return false;
	}
	struct Repeatable : RepeatIfPin<Repeatee::Repeatable, Pin>
	{
		using RepeatIfPin<Repeatee::Repeatable, Pin>::RepeatIfPin;
		bool Repeat() override
		{
			if (Repeatee::Repeatable::Repeat())
				return true;
			PinMonitors[Pin] = &MonitorCallback;
			return false;
		}
		using Repeatable = Repeatable;
	};
	void Pause() const override
	{
		Repeatee::Repeatable::Pause();
		PinMonitors.erase(Pin);
	}
	void Continue() const override
	{
		Repeatee::Repeatable::Continue();
		PinMonitors[Pin] = &MonitorCallback;
	}
	void Abort() const override
	{
		Repeatee::Repeatable::Abort();
		PinMonitors.erase(Pin);
	}
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << WriteStructSize(3) << WriteStepID(UID::Step_RepeatIfPin) << WriteField(Pin) << WriteStep(Repeatee);
	}

protected:
	std::move_only_function<void() const> const ChildCallback;
	std::move_only_function<void() const> const MonitorCallback;
};
template <typename SwitchFrom, uint8_t Pin, typename SwitchTo>
struct _SwitchIfPin : Step
{
	void Pause() const override
	{
		Current->Pause();
		if (Current == &From)
			PinMonitors.erase(Pin);
	}
	void Continue() const override
	{
		Current->Continue();
		if (Current == &From)
			PinMonitors[Pin] = &MonitorCallback;
	}
	void Abort() const override
	{
		Current->Abort();
		if (Current == &From)
			PinMonitors.erase(Pin);
	}
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << WriteStructSize(4) << WriteStepID(UID::Step_SwitchIfPin) << WriteStep(SwitchFrom);
		OutStream << WriteField(Pin) << WriteStep(SwitchTo);
	}

protected:
	Step *Current;
	std::move_only_function<void() const> const FromCallback;
	std::move_only_function<void() const> const MonitorCallback;
	SwitchFrom From;
	SwitchTo To;
	_SwitchIfPin(std::move_only_function<void() const> const &ParentCallback, std::move_only_function<void() const> &&MonitorCallback) : FromCallback([ParentCallback]()
																																					  {
		PinMonitors.erase(Pin);
		ParentCallback(); }),
																																		 MonitorCallback(std::move(MonitorCallback)), From{FromCallback}, To{ParentCallback}
	{
		static_assert(!ContainTrials<SwitchFrom>::value, "SwitchIfPin步骤的SwitchFrom不能包含Trial");
		static_assert(!ContainTrials<SwitchTo>::value, "SwitchIfPin步骤的SwitchTo不能包含Trial");
	}
};
// 执行SwitchFrom步骤并监控引脚。如果SwitchFrom结束前检测到引脚电平，放弃SwitchFrom，转而执行SwitchTo步骤。可以不指定SwitchTo，则检测到电平后仅放弃SwitchFrom，然后本步骤结束。
template <typename SwitchFrom, uint8_t Pin, typename SwitchTo>
struct SwitchIfPin : _SwitchIfPin<SwitchFrom, Pin, SwitchTo>
{
	struct Repeatable : _SwitchIfPin<SwitchFrom::Repeatable, Pin, SwitchTo::Repeatable>
	{
		Repeatable(std::move_only_function<void() const> const &ParentCallback) : _SwitchIfPin<SwitchFrom::Repeatable, Pin, SwitchTo::Repeatable>(ParentCallback, [this, ParentCallback]()
																																				  {
			From.Abort();
			PinMonitors.erase(Pin);
			if (Repeating?To.Repeat():To.Start())
				ParentCallback();
			else
				Current = &To; }) {}
		bool Start() override
		{
			if (From.Start())
				return true;
			Current = &From;
			Repeating = false;
			PinMonitors[Pin] = &MonitorCallback;
			return false;
		}
		bool Repeat() override
		{
			if (From.Repeat())
				return true;
			PinMonitors[Pin] = &MonitorCallback;
			Current = &From;
			Repeating = true;
			return false;
		}
		using Repeatable = Repeatable;

	protected:
		bool Repeating;
	};
	SwitchIfPin(std::move_only_function<void() const> const &ParentCallback) : _SwitchIfPin<SwitchFrom, Pin, SwitchTo>(ParentCallback, [this, ParentCallback]()
																													   {
		From.Abort();
		PinMonitors.erase(Pin);
		if (To.Start())
			ParentCallback();
		else
			Current = &To; }) {}
	bool Start() override
	{
		if (From.Start())
			return true;
		Current = &From;
		PinMonitors[Pin] = &MonitorCallback;
		return false;
	}
};
// 执行SwitchFrom步骤并监控引脚，如果SwitchFrom结束前检测到引脚电平，放弃执行，结束步骤。
template <typename SwitchFrom, uint8_t Pin>
using AbortIfPin = SwitchIfPin<SwitchFrom, Pin, NullStep>;
template <typename Unconditional, uint8_t Pin, typename Conditional>
struct _AppendIfPin : Step
{
	void Pause() const override
	{
		Current->Pause();
		if (Current == &UnconditionalStep)
			PinMonitors.erase(Pin);
	}
	void Continue() const override
	{
		Current->Continue();
		if (Current == &UnconditionalStep)
			PinMonitors[Pin] = &MonitorCallback;
	}
	void Abort() const override
	{
		Current->Abort();
		if (Current == &UnconditionalStep)
			PinMonitors.erase(Pin);
	}

protected:
	Step *Current;
	bool PinDetected;
	std::move_only_function<void() const> const MonitorCallback{[&PinDetected]()
																{ PinDetected = true; }};
	std::move_only_function<void() const> const UnconditionalCallback;
	Unconditional UnconditionalStep;
	Conditional ConditionalStep;
	_AppendIfPin(std::move_only_function<void() const> const &ParentCallback, std::move_only_function<void() const> &&UnconditionalCallback) : UnconditionalCallback(std::move(UnconditionalCallback)), UnconditionalStep{UnconditionalCallback}, ConditionalStep{ParentCallback} {}
};
// 执行Unconditional步骤并监控引脚，如果Unconditional结束前检测到引脚电平，则在Unconditional结束后额外执行Conditional步骤。
template <typename Unconditional, uint8_t Pin, typename Conditional>
struct AppendIfPin : Step
{
	struct Repeatable : _AppendIfPin<Unconditional::Repeatable, Pin, Conditional::Repeatable>
	{
		Repeatable(std::move_only_function<void() const> const &ParentCallback) : _AppendIfPin<Unconditional::Repeatable, Pin, Conditional::Repeatable>(ParentCallback, [this, ParentCallback]()
																																						{
			PinMonitors.erase(Pin);
			if (!PinDetected||(Repeating?ConditionalStep.Repeat():ConditionalStep.Start()))
				ParentCallback();
			else
				Current=&ConditionalStep; }) {}
		bool Start() override
		{
			if (UnconditionalStep.Start())
				return true;
			Current = &UnconditionalStep;
			PinDetected = false;
			PinMonitors[Pin] = &MonitorCallback;
			Repeating = false;
			return false;
		}
		bool Repeat() override
		{
			if (UnconditionalStep.Repeat())
				return true;
			PinMonitors[Pin] = &MonitorCallback;
			PinDetected = false;
			Current = &UnconditionalStep;
			Repeating = true;
			return false;
		}

	protected:
		bool Repeating;
	};
	AppendIfPin(std::move_only_function<void() const> const &ParentCallback) : _AppendIfPin<Unconditional, Pin, Conditional>(ParentCallback, [this, ParentCallback]()
																															 {
		PinMonitors.erase(Pin);
		if (!PinDetected || ConditionalStep.Start())
			ParentCallback();
		else
			Current = &ConditionalStep; }) {}
	bool Start() override
	{
		if (UnconditionalStep.Start())
			return true;
		Current = &UnconditionalStep;
		PinDetected = false;
		PinMonitors[Pin] = &MonitorCallback;
		return false;
	}
};
// 使用Async包装的步骤将异步执行，不等待那个步骤结束，Async就立即结束返回，那个步骤在后台自动执行直到结束，因而也无法暂停或放弃。
template <typename AsyncStep>
struct Async : AsyncStep
{
	Async(std::move_only_function<void() const> const &) : AsyncStep(NullCallback) {}
	bool Start() override
	{
		AsyncStep::Start();
		return true;
	}
	struct Repeatable : Async<AsyncStep::Repeatable>
	{
		using Async<AsyncStep::Repeatable>::Async;
		bool Repeat() override
		{
			AsyncStep::Repeatable::Repeat();
			return true;
		}
	};
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << WriteStructSize(2) << WriteStepID(UID::Step_Async) << WriteStep(AsyncStep);
	}
};
template <bool V, bool... Vs>
struct Any
{
	static constexpr bool value = V || Any<Vs...>::value;
};
template <bool V>
struct Any<V>
{
	static constexpr bool value = V;
};
template <typename Indices>
struct CopyTupleToPointers;
template <size_t... Indices>
struct CopyTupleToPointers<std::index_sequence<Indices...>>
{
	template <typename... Types>
	static void Copy(std::tuple<Types...> const &Source, Step *const *Destination)
	{
		Step *const _[] = {Destination[Indices] = &std::get<Indices>(Source)...};
	}
};
template <typename... Steps>
struct Sequential : Step
{
	static constexpr bool ContainTrials = Any<ContainTrials<Steps>::value...>::value;
	std::tuple<Steps...> StepsTuple;
	Step *const StepPointers[sizeof...(Steps)];
	Step *const *CurrentStep;
	std::move_only_function<void() const> const ChildCallback;
	std::unordered_map<UID, uint16_t> *TrialsDoneLeft;
	Sequential(std::move_only_function<void() const> const &ParentCallback) : ChildCallback{ContainTrials?[ParentCallback, this]()
	{
		while (++CurrentStep < std::end(StepPointers))
		if(TrialsDoneLeft)
		{
			bool const Finished=CurrentStep->Restore(*TrialsDoneLeft);
			if(TrialsDoneLeft)
		}
			if (!CurrentStep->Start())
				return;
		ParentCallback();
	}:[this, ParentCallback]()
	{
		while (++CurrentStep < std::end(StepPointers))
			if (!CurrentStep->Start())
				return;
		ParentCallback();
	}},
																			  StepsTuple{Steps{ChildCallback}...}
	{
		CopyTupleToPointers<std::make_index_sequence<sizeof...(Steps)>>::Copy(StepsTuple, StepPointers);
	}
	bool Start() override
	{
		for (CurrentStep = std::begin(StepPointers); CurrentStep < std::end(StepPointers); ++CurrentStep)
			if (!CurrentStep->Start())
			{
				TrialsDoneLeft = nullptr;
				return false;
			}
		return true;
	}
	void Pause() const override
	{
		CurrentStep->Pause();
	}
	void Continue() const override
	{
		CurrentStep->Continue();
	}
	void Abort() const override
	{
		CurrentStep->Abort();
	}
	bool Restore(std::unordered_map<UID, uint16_t> &TrialsDone) override
	{
		for (CurrentStep = std::begin(StepPointers); CurrentStep < std::end(StepPointers); ++CurrentStep)
			if (!CurrentStep->Restore(TrialsDone))
			{
				TrialsDoneLeft = &TrialsDone;
				return false;
			}
		return true;
	}
};
#define UidPairClass(Uid, Class) {UID::Uid, []() { return new Class; }}