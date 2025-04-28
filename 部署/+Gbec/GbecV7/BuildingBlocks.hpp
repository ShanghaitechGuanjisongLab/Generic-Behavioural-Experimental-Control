#pragma once
#include "UID.hpp"
#include "Timers_one_for_all.hpp"
#include <Quick_digital_IO_interrupt.hpp>
#include <chrono>
#include <unordered_map>
#include <ostream>
#include <cmath>
#include <random>
#include <set>
#include "Async_stream_IO.hpp"
struct Process {
	template <typename StepType>
	static Process* New() {
		Process* NewProcess = new Process();
		NewProcess->Content = std::make_unique<StepType>(NewProcess->ChildCallback, NewProcess);
		Existing.insert(NewProcess);
		return NewProcess;
	}
	bool Start() {
		RepeatLeft = 0;
		return Content->Start();
	}
	bool Restore(std::unordered_map<UID, uint16_t>&& TD) {
		return Content->Restore(TrialsDone = std::move(TD));
	}
	bool Repeat(uint16_t Times) {
		for (;;) {
			if (!Content->Start()) {
				RepeatLeft = Times - 1;
				return false;
			}
			if (!--Times)
				return true;
		}
	}
	void Pause() const {
		Content->Pause();
	}
	void Continue() const {
		Content->Continue();
	}
	void Abort() const {
		Content->Abort();
	}
	void WriteInfo(std::ostream& OutStream) const {
		Content->WriteInfoD(OutStream);
	}
	static std::set<Process*> Existing;

protected:
	std::move_only_function<void() const> const ChildCallback{ [this]() {
		while (RepeatLeft) {
			RepeatLeft--;
			if (!Content->Start())
				return;
		}
		Async_stream_IO::Send(this, static_cast<uint8_t>(UID::PortC_ProcessFinished));
	} };
	std::unique_ptr<Step> Content;
	std::unordered_map<UID, uint16_t> TrialsDone;
	uint16_t RepeatLeft;
	Process() {}
};
// 通用步骤接口。实际步骤不一定实现所有方法。
struct Step {
	// 返回true表示步骤已完成可以进入下一步，返回false表示步骤未完成需要等待FinishCallback。如果不重写此方法，此方法将调用Repeat然后返回true。
	virtual bool Start() {
		return true;
	}
	// 类似于Start，但必须重复和上次Start或Repeat完全相同的行为。这仅对于那些存在随机性的步骤有意义，对于确定性步骤此方法应当与Start做完全相同的事情。
	virtual bool Repeat() {
		return Start();
	}
	// 暂停当前步骤。不能暂停的步骤可以不override此方法。
	virtual void Pause() const {}
	// 继续已暂停的步骤。不能暂停的步骤可以不override此方法。
	virtual void Continue() const {}
	// 放弃当前步骤。不能放弃的步骤可以不override此方法。
	virtual void Abort() const {}
	// 从断线重连中恢复。不能恢复的步骤可以不override此方法，默认什么都不做直接返回true。
	virtual bool Restore(std::unordered_map<UID, uint16_t>& TrialsDone) {
		return true;
	}
	virtual ~Step() 
	{
		Abort();
	}
	// 将步骤信息写入流中
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << static_cast<uint8_t>(UID::Type_Empty);
	}
	virtual void WriteInfoD(std::ostream& OutStream) const {
		WriteInfoS(OutStream);
	}
};
using NullStep = Step;
template <typename>
struct TypeToUID;
template <>
struct TypeToUID<uint8_t> {
	static constexpr UID Value = UID::Type_UInt8;
};
#define WriteStructSize(Size) static_cast<uint8_t>(UID::Type_Struct) << static_cast<uint8_t>(Size)
#define WriteField(Property) static_cast<uint8_t>(UID::Property_##Property) << static_cast<uint8_t>(TypeToUID<decltype(Property)>::Value) << Property
#define WriteStepID(ID) static_cast<uint8_t>(UID::Property_StepID) << static_cast<uint8_t>(UID::Type_UID) << static_cast<uint8_t>(ID)
// 向引脚写出电平
template <uint8_t Pin, bool HighOrLow>
struct DigitalWrite : Step {
	DigitalWrite(std::move_only_function<void() const> const&, Process const*) {
		Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
	}
	bool Start() override {
		Quick_digital_IO_interrupt::DigitalWrite<Pin, HighOrLow>();
		return true;
	}
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(3) << WriteStepID(UID::Step_DigitalWrite) << WriteField(Pin) << WriteField(HighOrLow);
	}
	using Repeatable = DigitalWrite<Pin, HighOrLow>;
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}
};

#pragma pack(push, 1)
struct ProcessSignal {
	Process const* P;
	UID V;
};
#pragma pack(pop)

// 向串口写出UID
template <UID Value>
struct SerialWrite : Step {
	SerialWrite(std::move_only_function<void() const> const&, Process const* Container) :Container(Container) {}
	bool Start() override {
		Async_stream_IO::Send(ProcessSignal{ Container,Value }, static_cast<uint8_t>(UID::PortC_Signal));
		return true;
	}
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(2) << WriteStepID(UID::Step_SerialWrite) << WriteField(Value);
	}
	using Repeatable = SerialWrite<Value>;
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}
protected:
	Process const* const Container;
};

template <uint16_t Count>
struct Milliseconds {
	static constexpr std::chrono::milliseconds Get() { return std::chrono::milliseconds(Count) }
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << static_cast<uint8_t>(UID::Type_Milliseconds) << Count;
	}
};
template <uint8_t Count>
struct Seconds {
	static constexpr std::chrono::seconds Get() { return std::chrono::milliseconds(Count) }
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << static_cast<uint8_t>(UID::Type_Seconds) << Count;
	}
};
template <typename Min, typename Max>
class RandomDuration {
	using DurationType = decltype(Min::Get());

public:
	static DurationType Get() {
		constexpr float MinCount = Min::Get().count();
		return DurationType(pow(std::chrono::duration_cast<DurationType>(Min::Get()).count() / MinCount, random() / RANDOM_MAX) * MinCount);
	}
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(2) << static_cast<uint8_t>(UID::Property_Min);
		Min::WriteInfoS(OutStream);
		OutStream << static_cast<uint8_t>(UID::Property_Max);
		Max::WriteInfoS(OutStream);
	}
};
struct InfiniteDuration;
#define WriteDuration(FieldName, Duration) static_cast<uint8_t>(UID::Property_##FieldName) << static_cast<uint8_t>(Duration::Type) << Duration::Value.count()
// 等待一段时间，不做任何事
template <typename Duration>
struct Delay : Step {
	Delay(std::move_only_function<void() const> const& FinishCallback, Process const*) : TimerCallback([this]() {
		Timer->Allocatable = true;
		FinishCallback(); }) {
	}
	bool Start() override {
		(Timer = Timers_one_for_all::AllocateTimer())->DoAfter(Duration::Get(), TimerCallback);
		return false;
	}
	void Pause() const override {
		Timer->Pause();
	}
	void Continue() const override {
		Timer->Continue();
	}
	void Abort() const override {
		Timer->Stop();
		Timer->Allocatable = true;
	}
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(2) << WriteStepID(UID::Step_Delay) << WriteDuration(Duration, Duration);
	}
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}
	struct Repeatable : Delay<Duration> {
		using Delay<Duration>::Delay;
		bool Start() override {
			(Timer = Timers_one_for_all::AllocateTimer())->DoAfter(LastDuration = Duration::Get(), TimerCallback);
			return false;
		}
		bool Repeat() override {
			(Timer = Timers_one_for_all::AllocateTimer())->DoAfter(LastDuration, TimerCallback);
			return false;
		}

	protected:
		decltype(Duration::Get()) LastDuration;
	};

protected:
	std::move_only_function<void() const> const TimerCallback;
	Timers_one_for_all::TimerClass* Timer;
};
template <>
struct Delay<InfiniteDuration> : Step {
	Delay(std::move_only_function<void() const> const&, Process const*) {}
	bool Start() override {
		return false;
	}
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(2) << WriteStepID(UID::Step_Delay) << static_cast<uint8_t>(UID::Property_Duration) << static_cast<uint8_t>(UID::Type_Infinite);
	}
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}
	using Repeatable = Delay<InfiniteDuration>;
};

extern std::move_only_function<void() const> const NullCallback;
#define WriteStep(FieldName)                         \
	static_cast<uint8_t>(UID::Property_##FieldName); \
	FieldName::WriteInfoS(OutStream);

// 引脚监视模块
extern std::set<std::move_only_function<void() const> const*> _PendingInterrupts;
template <uint8_t Pin>
struct _PinInterrupt {
	static std::set<std::move_only_function<void() const> const*> Handlers;
	static void AddHandler(std::move_only_function<void() const> const& Handler) {
		if (Handlers.empty())
			Quick_digital_IO_interrupt::AttachInterrupt<RISING>(Pin, []() { for (auto H : Handlers)
				_PendingInterrupts.insert(H); });
		Handlers.insert(&Handler);
	}
	static void RemoveHandler(std::move_only_function<void() const> const& Handler) {
		Handlers.erase(&Handler);
		if (Handlers.empty())
			Quick_digital_IO_interrupt::DetachInterrupt(Pin);
		_PendingInterrupts.erase(&Handler);
	}
};
template <uint8_t Pin>
std::set<std::move_only_function<void() const> const*> _PinInterrupt<Pin>::Handlers;

template <typename StepType, typename = bool>
struct _ContainTrials {
	static constexpr bool value = false;
};
template <typename StepType>
struct _ContainTrials<StepType, decltype(StepType::_iContainTrials)> {
	static constexpr bool value = StepType::_iContainTrials;
};
template <UID TrialID, typename TrialStep>
struct Trial : TrialStep {
	static constexpr bool _iContainTrials = true;
	Trial(std::move_only_function<void() const> const& FinishCallback, Process const* Container) : TrialStep(FinishCallback, Container), Signal{ Container, TrialID } {
		static_assert(!_ContainTrials<TrialStep>::value, "Trial步骤的TrialStep不能包含Trial");
	}
	bool Start() override {
		Async_stream_IO::Send(Signal, static_cast<uint8_t>(UID::PortC_TrialStart));
		return TrialStep::Start();
	}
	struct Repeatable : Trial<TrialID, TrialStep::Repeatable> {
		using Trial<TrialID, TrialStep::Repeatable>::Trial;
		bool Repeat() override {
			Async_stream_IO::Send(Signal, , static_cast<uint8_t>(UID::PortC_TrialStart));
			return TrialStep::Repeatable::Repeat();
		}
		using Repeatable = Repeatable;
	};
	bool Restore(std::unordered_map<UID, uint16_t>& TrialsDone) override {
		auto const it = TrialsDone.find(TrialID);
		if (it == TrialsDone.end())
			return Start();
		if (it->second) {
			if (!--it->second)
				TrialsDone.erase(it);
			return true;
		}
		else {
			TrialsDone.erase(it);
			return Start();
		}
	}
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(3) << WriteStepID(UID::Step_Trial) << WriteField(TrialID) << WriteStep(TrialStep);
	}
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}
protected:
	ProcessSignal const Signal;
};
// 执行Repeatee步骤，同时监控引脚。如果Repeatee执行完之前检测到引脚电平，放弃当前执行并重启。Repeatee步骤不能包含Trial。对于包含随机内容的步骤，将不会重新抽取随机，而是保持一致。
template <typename Repeatee, uint8_t Pin>
struct RepeatIfPin : Repeatee::Repeatable // 有些步骤的Repeatable不继承自那个步骤类型
{
	RepeatIfPin(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : ChildCallback{ [ParentCallback]() {
			_PinInterrupt<Pin>::RemoveHandler(MonitorCallback);
			ParentCallback(); } },
		Repeatee::Repeatable(ChildCallback, Container), MonitorCallback{ [this]() {
																 Repeatee::Repeatable::Abort();
																 Repeatee::Repeatable::Repeat(); } }
	{
		static_assert(!_ContainTrials<Repeatee::Repeatable>::value, "RepeatIfPin步骤的Repeatee不能包含Trial");
	}
	bool Start() override {
		if (Repeatee::Repeatable::Start())
			return true;
		_PinInterrupt<Pin>::AddHandler(MonitorCallback);
		return false;
	}
	struct Repeatable : RepeatIfPin<Repeatee::Repeatable, Pin> {
		using RepeatIfPin<Repeatee::Repeatable, Pin>::RepeatIfPin;
		bool Repeat() override {
			if (Repeatee::Repeatable::Repeat())
				return true;
			_PinInterrupt<Pin>::AddHandler(MonitorCallback);
			return false;
		}
		using Repeatable = Repeatable;
	};
	void Pause() const override {
		Repeatee::Repeatable::Pause();
		_PinInterrupt<Pin>::RemoveHandler(MonitorCallback);
	}
	void Continue() const override {
		Repeatee::Repeatable::Continue();
		_PinInterrupt<Pin>::AddHandler(MonitorCallback);
	}
	void Abort() const override {
		Repeatee::Repeatable::Abort();
		_PinInterrupt<Pin>::RemoveHandler(MonitorCallback);
	}
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(3) << WriteStepID(UID::Step_RepeatIfPin) << WriteField(Pin) << WriteStep(Repeatee);
	}
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}

protected:
	std::move_only_function<void() const> const ChildCallback;
	std::move_only_function<void() const> const MonitorCallback;
};
template <typename SwitchFrom, uint8_t Pin, typename SwitchTo>
struct _SwitchIfPin : Step {
	void Pause() const override {
		Current->Pause();
		if (Current == &From)
			_PinInterrupt<Pin>::RemoveHandler(MonitorCallback);
	}
	void Continue() const override {
		Current->Continue();
		if (Current == &From)
			_PinInterrupt<Pin>::AddHandler(MonitorCallback);
	}
	void Abort() const override {
		Current->Abort();
		if (Current == &From)
			_PinInterrupt<Pin>::RemoveHandler(MonitorCallback);
	}
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(4) << WriteStepID(UID::Step_SwitchIfPin) << WriteStep(SwitchFrom);
		OutStream << WriteField(Pin) << WriteStep(SwitchTo);
	}
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}

protected:
	Step* Current;
	std::move_only_function<void() const> const FromCallback;
	std::move_only_function<void() const> const MonitorCallback;
	SwitchFrom From;
	SwitchTo To;
	_SwitchIfPin(std::move_only_function<void() const> const& ParentCallback, std::move_only_function<void() const>&& MonitorCallback, Process const* Container) : FromCallback([ParentCallback]() {
		_PinInterrupt<Pin>::RemoveHandler(MonitorCallback);
		ParentCallback(); }),
		MonitorCallback(std::move(MonitorCallback)), From{ FromCallback,Container }, To{ ParentCallback,Container } {
		static_assert(!_ContainTrials<SwitchFrom>::value, "SwitchIfPin步骤的SwitchFrom不能包含Trial");
		static_assert(!_ContainTrials<SwitchTo>::value, "SwitchIfPin步骤的SwitchTo不能包含Trial");
	}
};
// 执行SwitchFrom步骤并监控引脚。如果SwitchFrom结束前检测到引脚电平，放弃SwitchFrom，转而执行SwitchTo步骤。可以不指定SwitchTo，则检测到电平后仅放弃SwitchFrom，然后本步骤结束。
template <typename SwitchFrom, uint8_t Pin, typename SwitchTo>
struct SwitchIfPin : _SwitchIfPin<SwitchFrom, Pin, SwitchTo> {
	struct Repeatable : _SwitchIfPin<SwitchFrom::Repeatable, Pin, SwitchTo::Repeatable> {
		Repeatable(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : _SwitchIfPin<SwitchFrom::Repeatable, Pin, SwitchTo::Repeatable>(ParentCallback, [this, ParentCallback]() {
			From.Abort();
			_PinInterrupt<Pin>::RemoveHandler(MonitorCallback);
			if (Repeating ? To.Repeat() : To.Start())
				ParentCallback();
			else
				Current = &To; }, Container) {
		}
		bool Start() override {
			if (From.Start())
				return true;
			Current = &From;
			Repeating = false;
			_PinInterrupt<Pin>::AddHandler(MonitorCallback);
			return false;
		}
		bool Repeat() override {
			if (From.Repeat())
				return true;
			_PinInterrupt<Pin>::AddHandler(MonitorCallback);
			Current = &From;
			Repeating = true;
			return false;
		}
		using Repeatable = Repeatable;

	protected:
		bool Repeating;
	};
	SwitchIfPin(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : _SwitchIfPin<SwitchFrom, Pin, SwitchTo>(ParentCallback, [this, ParentCallback]() {
		From.Abort();
		_PinInterrupt<Pin>::RemoveHandler(MonitorCallback);
		if (To.Start())
			ParentCallback();
		else
			Current = &To; }, Container) {
	}
	bool Start() override {
		if (From.Start())
			return true;
		Current = &From;
		_PinInterrupt<Pin>::AddHandler(MonitorCallback);
		return false;
	}
};
// 执行SwitchFrom步骤并监控引脚，如果SwitchFrom结束前检测到引脚电平，放弃执行，结束步骤。
template <typename SwitchFrom, uint8_t Pin>
using AbortIfPin = SwitchIfPin<SwitchFrom, Pin, NullStep>;
template <typename Unconditional, uint8_t Pin, typename Conditional>
struct _AppendIfPin : Step {
	void Pause() const override {
		Current->Pause();
		if (Current == &UnconditionalStep)
			_PinInterrupt<Pin>::RemoveHandler(MonitorCallback);
	}
	void Continue() const override {
		Current->Continue();
		if (Current == &UnconditionalStep)
			_PinInterrupt<Pin>::AddHandler(MonitorCallback);
	}
	void Abort() const override {
		Current->Abort();
		if (Current == &UnconditionalStep)
			_PinInterrupt<Pin>::RemoveHandler(MonitorCallback);
	}

protected:
	Step* Current;
	bool PinDetected;
	std::move_only_function<void() const> const MonitorCallback{ [&PinDetected]() { PinDetected = true; } };
	std::move_only_function<void() const> const UnconditionalCallback;
	Unconditional UnconditionalStep;
	Conditional ConditionalStep;
	_AppendIfPin(std::move_only_function<void() const> const& ParentCallback, std::move_only_function<void() const>&& UnconditionalCallback, Process const* Container) : UnconditionalCallback(std::move(UnconditionalCallback)), UnconditionalStep{ UnconditionalCallback,Container }, ConditionalStep{ ParentCallback,Container } {
		static_assert(!_ContainTrials<Unconditional>::value, "AppendIfPin步骤的Unconditional不能包含Trial");
		static_assert(!_ContainTrials<Conditional>::value, "AppendIfPin步骤的Conditional不能包含Trial");
	}
};
// 执行Unconditional步骤并监控引脚，如果Unconditional结束前检测到引脚电平，则在Unconditional结束后额外执行Conditional步骤。
template <typename Unconditional, uint8_t Pin, typename Conditional>
struct AppendIfPin : Step {
	struct Repeatable : _AppendIfPin<Unconditional::Repeatable, Pin, Conditional::Repeatable> {
		Repeatable(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : _AppendIfPin<Unconditional::Repeatable, Pin, Conditional::Repeatable>(ParentCallback, [this, ParentCallback]() {
			_PinInterrupt<Pin>::RemoveHandler(MonitorCallback);
			if (!PinDetected || (Repeating ? ConditionalStep.Repeat() : ConditionalStep.Start()))
				ParentCallback();
			else
				Current = &ConditionalStep; }, Container) {
		}
		bool Start() override {
			if (UnconditionalStep.Start())
				return true;
			Current = &UnconditionalStep;
			PinDetected = false;
			_PinInterrupt<Pin>::AddHandler(MonitorCallback);
			Repeating = false;
			return false;
		}
		bool Repeat() override {
			if (UnconditionalStep.Repeat())
				return true;
			_PinInterrupt<Pin>::AddHandler(MonitorCallback);
			PinDetected = false;
			Current = &UnconditionalStep;
			Repeating = true;
			return false;
		}

	protected:
		bool Repeating;
	};
	AppendIfPin(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : _AppendIfPin<Unconditional, Pin, Conditional>(ParentCallback, [this, ParentCallback]() {
		_PinInterrupt<Pin>::RemoveHandler(MonitorCallback);
		if (!PinDetected || ConditionalStep.Start())
			ParentCallback();
		else
			Current = &ConditionalStep; }, Container) {
	}
	bool Start() override {
		if (UnconditionalStep.Start())
			return true;
		Current = &UnconditionalStep;
		PinDetected = false;
		_PinInterrupt<Pin>::AddHandler(MonitorCallback);
		return false;
	}
};
// 使用Async包装的步骤将异步执行，不等待那个步骤结束，Async就立即结束返回，那个步骤在后台自动执行直到结束，因而也无法暂停或放弃。
template <typename AsyncStep>
struct Async : AsyncStep {
	Async(std::move_only_function<void() const> const&, Process const* Container) : AsyncStep(NullCallback, Container) {
		static_assert(!_ContainTrials<AsyncStep>::value, "Async步骤的AsyncStep不能包含Trial");
	}
	bool Start() override {
		AsyncStep::Start();
		return true;
	}
	struct Repeatable : Async<AsyncStep::Repeatable> {
		using Async<AsyncStep::Repeatable>::Async;
		bool Repeat() override {
			AsyncStep::Repeatable::Repeat();
			return true;
		}
	};
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(2) << WriteStepID(UID::Step_Async) << WriteStep(AsyncStep);
	}
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}
};
template <bool V, bool... Vs>
struct _Any {
	static constexpr bool value = V || _Any<Vs...>::value;
};
template <bool V>
struct _Any<V> {
	static constexpr bool value = V;
};
template <bool V, bool... Vs>
struct _All {
	static constexpr bool value = V && _Any<Vs...>::value;
};
template <bool V>
struct _All<V> {
	static constexpr bool value = V;
};
struct _StepWithRepeat {
	Step* StepPointer;
	uint16_t RepeatCount;
};
template <typename Indices>
struct _CopyTupleToPointers;
template <size_t... Indices>
struct _CopyTupleToPointers<std::index_sequence<Indices...>> {
	template <typename... Types>
	static void Copy(std::tuple<Types...> const& Source, Step* const* Destination) {
		Step* const _[] = { Destination[Indices] = &std::get<Indices>(Source)... };
	}
	template <typename... Types>
	static void Copy(std::tuple<Types...> const& Source, _StepWithRepeat* Destination) {
		Step* const _[] = { Destination[Indices].StepPointer = &std::get<Indices>(Source)... };
	}
};

template <typename... Steps>
struct _Sequential_Base : Step {
	_Sequential_Base(std::move_only_function<void() const>&& ChildCallback, Process const* Container) : ChildCallback{ std::move(ChildCallback) }, StepsTuple{ Steps{ChildCallback,Container}... } {
		_CopyTupleToPointers<std::make_index_sequence<sizeof...(Steps)>>::Copy(StepsTuple, StepPointers);
	}
	bool Start() override {
		for (CurrentStep = std::begin(StepPointers); CurrentStep < std::end(StepPointers); ++CurrentStep)
			if (!CurrentStep->Start())
				return false;
		return true;
	}
	void Pause() const override {
		CurrentStep->Pause();
	}
	void Continue() const override {
		CurrentStep->Continue();
	}
	void Abort() const override {
		CurrentStep->Abort();
	}
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(2) << WriteStepID(UID::Step_Sequential) << static_cast<uint8_t>(UID::Property_Steps) << static_cast<uint8_t>(UID::Type_Cell) << static_cast<uint8_t>(sizeof...(Steps));
		int _[] = { (Steps::WriteInfoS(OutStream), 0)... };
	}
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}

protected:
	std::tuple<Steps...> StepsTuple;
	Step* StepPointers[sizeof...(Steps)];
	Step* const* CurrentStep;
	std::move_only_function<void() const> const ChildCallback;
};
template <typename... Steps>
struct _Sequential_Simple : _Sequential_Base<Steps...> {
	_Sequential_Simple(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : _Sequential_Base<Steps...>([ParentCallback, this]() {
		while (++CurrentStep < std::end(StepPointers))
			if (!CurrentStep->Start())
				return;
		ParentCallback(); }, Container) {
	}
	struct Repeatable : _Sequential_Base<Steps...> {
		Repeatable(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : _Sequential_Base<Steps...>([ParentCallback, this]() {
			while (++CurrentStep < std::end(StepPointers))
				if (!(Repeating ? CurrentStep->Repeat() : CurrentStep->Start()))
					return;
			ParentCallback(); }, Container) {
		}
		bool Start() override {
			Repeating = false;
			for (CurrentStep = std::begin(StepPointers); CurrentStep < std::end(StepPointers); ++CurrentStep)
				if (!CurrentStep->Start())
					return false;
			return true;
		}
		bool Repeat() override {
			Repeating = true;
			for (CurrentStep = std::begin(StepPointers); CurrentStep < std::end(StepPointers); ++CurrentStep)
				if (!CurrentStep->Repeat())
					return false;
			return true;
		}

	protected:
		bool Repeating;
	};
};
template <typename... Steps>
struct _Sequential_WithTrials : _Sequential_Base<Steps...> {
	static constexpr bool _ContainTrials = true;
	_Sequential_WithTrials(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : _Sequential_Base<Steps...>([ParentCallback, this]() {
		while (++CurrentStep < std::end(StepPointers))
			if (TrialsDoneLeft) {
				bool const Finished = CurrentStep->Restore(*TrialsDoneLeft);
				if (TrialsDoneLeft->empty())
					TrialsDoneLeft = nullptr;
				if (!Finished)
					return;
			}
			else if (!CurrentStep->Start())
				return;
		ParentCallback(); }, Container) {
	}
	bool Start() override {
		for (CurrentStep = std::begin(StepPointers); CurrentStep < std::end(StepPointers); ++CurrentStep)
			if (!CurrentStep->Start()) {
				TrialsDoneLeft = nullptr;
				return false;
			}
		return true;
	}
	bool Restore(std::unordered_map<UID, uint16_t>& TrialsDone) override {
		for (CurrentStep = std::begin(StepPointers); CurrentStep < std::end(StepPointers); ++CurrentStep)
			if (!CurrentStep->Restore(TrialsDone)) {
				TrialsDoneLeft = &TrialsDone;
				return false;
			}
		return true;
	}
	struct Repeatable {
		static_assert(false, "Sequential::Repeatable不允许包含Trial");
	};
	// 扩展指定每个步骤的重复次数。上一个步骤的所有重复结束后才会执行下一个步骤。只有包含Trial的步骤支持重复。
	template <uint16_t... Repeats>
	struct WithRepeats : Step {
		static constexpr bool _ContainTrials = true;
		WithRepeats(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : ChildCallback{ [ParentCallback, this]() {
																									 for (;;) {
																										 if (!--CurrentStep->RepeatCount && ++CurrentStep == std::end(StepPointers)) {
																											 ParentCallback();
																											 return;
																										 }
																										 if (TrialsDoneLeft) {
																											 bool const Finished = CurrentStep->StepPointer->Restore(*TrialsDoneLeft);
																											 if (TrialsDoneLeft->empty())
																												 TrialsDoneLeft = nullptr;
																											 if (!Finished)
																												 break;
																										 }
																										 else if (!CurrentStep->StepPointer->Start())
																											 break;
																									 }
																								 } },
			StepsTuple{ Steps{ChildCallback,Container}... } {
			static_assert(sizeof...(Steps) == sizeof...(Repeats), "WithRepeats的Steps和Repeats数量不匹配");
			static_assert(!_All<Repeats>::value, "WithRepeats的Repeats不能包含0");
			_CopyTupleToPointers<std::make_index_sequence<sizeof...(Steps)>>::Copy(StepsTuple, StepPointers);
		}
		bool Start() override {
			CurrentStep = std::begin(StepPointers);
			uint16_t const _[] = { CurrentStep++->RepeatCount = Repeats... };
			CurrentStep = std::begin(StepPointers);
			while (CurrentStep->StepPointer->Start())
				if (!--CurrentStep->RepeatCount && ++CurrentStep == std::end(StepPointers))
					return true;
			TrialsDoneLeft = nullptr;
			return false;
		}
		bool Restore(std::unordered_map<UID, uint16_t>& TrialsDone) override {
			CurrentStep = std::begin(StepPointers);
			uint16_t const _[] = { CurrentStep++->RepeatCount = Repeats... };
			CurrentStep = std::begin(StepPointers);
			while (CurrentStep->StepPointer->Restore(TrialsDone)) {
				if (!--CurrentStep->RepeatCount && ++CurrentStep == std::end(StepPointers))
					return true;
				if (TrialsDoneLeft->empty()) {
					while (CurrentStep->StepPointer->Start())
						if (!--CurrentStep->RepeatCount && ++CurrentStep == std::end(StepPointers))
							return true;
					TrialsDoneLeft = nullptr;
					return false;
				}
			}
			TrialsDoneLeft = &TrialsDone;
			return false;
		}
		void Pause() const override {
			CurrentStep->StepPointer->Pause();
		}
		void Continue() const override {
			CurrentStep->StepPointer->Continue();
		}
		void Abort() const override {
			CurrentStep->StepPointer->Abort();
		}
		static void WriteInfoS(std::ostream& OutStream) {
			OutStream << WriteStructSize(2) << WriteStepID(UID::Step_Sequential) << static_cast<uint8_t>(UID::Property_Steps) << static_cast<uint8_t>(UID::Type_Array) << static_cast<uint8_t>(sizeof...(Steps));
			int _[] = { (OutStream << WriteStructSize(2) << static_cast<uint8_t>(UID::Property_Step), Steps::WriteInfoS(OutStream), OutStream << static_cast<uint8_t>(UID::Property_Repeat) << static_cast<uint8_t>(UID::Type_UInt16) << Repeats, 0)... };
		}
		void WriteInfoD(std::ostream& OutStream) const override {
			WriteInfoS(OutStream);
		}
		struct Repeatable {
			static_assert(false, "Sequential::WithRepeats不允许Repeatable");
		};

	protected:
		std::tuple<Steps...> StepsTuple;
		_StepWithRepeat StepPointers[sizeof...(Steps)];
		_StepWithRepeat* CurrentStep;
		std::move_only_function<void() const> const ChildCallback;
		std::unordered_map<UID, uint16_t>* TrialsDoneLeft;
	};

protected:
	std::unordered_map<UID, uint16_t>* TrialsDoneLeft;
};
template <typename... Steps>
using _Sequential_Selector = std::conditional_t<_Any<_ContainTrials<Steps>::value...>::value, _Sequential_WithTrials<Steps...>, _Sequential_Simple<Steps...>>;
// 按顺序执行步骤。支持扩展::WithRepeats，以将每个步骤重复执行多次
template <typename... Steps>
struct Sequential : _Sequential_Selector<Steps...> {
	using _Sequential_Selector<Steps...>::_Sequential_Selector;
};

#ifdef ARDUINO_ARCH_AVR
using ArchUrng = std::ArduinoUrng;
#else
using ArchUrng = std::TrueUrng;
#endif
extern ArchUrng Urng;
template <typename... Steps>
struct _Random_Base : Step {
	_Random_Base(std::move_only_function<void() const>&& ChildCallback, Process const* Container) : ChildCallback{ std::move(ChildCallback) }, StepsTuple{ Steps{ChildCallback,Container}... } {
		_CopyTupleToPointers<std::make_index_sequence<sizeof...(Steps)>>::Copy(StepsTuple, StepPointers);
	}
	bool Start() override {
		std::shuffle(std::begin(StepPointers), std::end(StepPointers), Urng);
		for (CurrentStep = std::begin(StepPointers); CurrentStep < std::end(StepPointers); ++CurrentStep)
			if (!CurrentStep->Start())
				return false;
		return true;
	}
	void Pause() const override {
		CurrentStep->Pause();
	}
	void Continue() const override {
		CurrentStep->Continue();
	}
	void Abort() const override {
		CurrentStep->Abort();
	}
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(2) << WriteStepID(UID::Step_Random) << static_cast<uint8_t>(UID::Property_Steps) << static_cast<uint8_t>(UID::Type_Cell) << static_cast<uint8_t>(sizeof...(Steps));
		int _[] = { (Steps::WriteInfoS(OutStream), 0)... };
	}
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}

protected:
	std::tuple<Steps...> StepsTuple;
	Step* StepPointers[sizeof...(Steps)];
	Step* const* CurrentStep;
	std::move_only_function<void() const> const ChildCallback;
};
template <typename... Steps>
struct _Random_Simple : _Random_Base<Steps...> {
	_Random_Simple(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : _Random_Base<Steps...>([ParentCallback, this]() {
		while (++CurrentStep < std::end(StepPointers))
			if (!CurrentStep->Start())
				return;
		ParentCallback(); }, Container) {
	}
	struct Repeatable : _Random_Base<Steps...> {
		Repeatable(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : _Random_Base<Steps...>([ParentCallback, this]() {
			while (++CurrentStep < std::end(StepPointers))
				if (!(Repeating ? CurrentStep->Repeat() : CurrentStep->Start()))
					return;
			ParentCallback(); }, Container) {
		}
		bool Start() override {
			Repeating = false;
			std::shuffle(std::begin(StepPointers), std::end(StepPointers), Urng);
			for (CurrentStep = std::begin(StepPointers); CurrentStep < std::end(StepPointers); ++CurrentStep)
				if (!CurrentStep->Start())
					return false;
			return true;
		}
		bool Repeat() override {
			Repeating = true;
			for (CurrentStep = std::begin(StepPointers); CurrentStep < std::end(StepPointers); ++CurrentStep)
				if (!CurrentStep->Repeat())
					return false;
			return true;
		}

	protected:
		bool Repeating;
	};
};
template <typename... Steps>
struct _Random_WithTrials : _Random_Base<Steps...> {
	static constexpr bool _ContainTrials = true;
	_Random_WithTrials(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : _Random_Base<Steps...>([ParentCallback, this]() {
		while (++CurrentStep < std::end(StepPointers))
			if (TrialsDoneLeft) {
				bool const Finished = CurrentStep->Restore(*TrialsDoneLeft);
				if (TrialsDoneLeft->empty())
					TrialsDoneLeft = nullptr;
				if (!Finished)
					return;
			}
			else if (!CurrentStep->Start())
				return;
		ParentCallback(); }, Container) {
	}
	bool Start() override {
		std::shuffle(std::begin(StepPointers), std::end(StepPointers), Urng);
		for (CurrentStep = std::begin(StepPointers); CurrentStep < std::end(StepPointers); ++CurrentStep)
			if (!CurrentStep->Start()) {
				TrialsDoneLeft = nullptr;
				return false;
			}
		return true;
	}
	bool Restore(std::unordered_map<UID, uint16_t>& TrialsDone) override {
		std::shuffle(std::begin(StepPointers), std::end(StepPointers), Urng);
		for (CurrentStep = std::begin(StepPointers); CurrentStep < std::end(StepPointers); ++CurrentStep)
			if (!CurrentStep->Restore(TrialsDone)) {
				TrialsDoneLeft = &TrialsDone;
				return false;
			}
		return true;
	}
	struct Repeatable {
		static_assert(false, "Random::Repeatable不允许包含Trial");
	};
	// 扩展指定每个步骤的随机重复次数。所有步骤将彼此随机穿插，最终重复各自指定的次数。
	template <uint16_t... Repeats>
	struct WithRepeats : Step {
		static constexpr bool _ContainTrials = true;
		WithRepeats(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : ChildCallback{ [ParentCallback, this]() {
																									 for (;;) {
																										 PickRandomStep();
																										 if (!CurrentStep) {
																											 ParentCallback();
																											 return;
																										 }
																										 if (TrialsDoneLeft) {
																											 bool const Finished = CurrentStep->Restore(*TrialsDoneLeft);
																											 if (TrialsDoneLeft->empty())
																												 TrialsDoneLeft = nullptr;
																											 if (!Finished)
																												 break;
																										 }
																										 else if (!CurrentStep->Start())
																											 break;
																									 }
																								 } },
			StepsTuple{ Steps{ChildCallback,Container}... } {
			static_assert(sizeof...(Steps) == sizeof...(Repeats), "WithRepeats的Steps和Repeats数量不匹配");
			static_assert(!_All<Repeats>::value, "WithRepeats的Repeats不能包含0");
			_CopyTupleToPointers<std::make_index_sequence<sizeof...(Steps)>>::Copy(StepsTuple, StepPointers);
		}
		bool Start() override {
			_StepWithRepeat* CurrentSR = std::begin(StepPointers);
			uint16_t const _[] = { CurrentSR++->RepeatCount = Repeats... };
			for (;;) {
				PickRandomStep();
				if (!CurrentStep)
					return true;
				if (!CurrentStep->Start()) {
					TrialsDoneLeft = nullptr;
					return false;
				}
			}
		}
		bool Restore(std::unordered_map<UID, uint16_t>& TrialsDone) override {
			_StepWithRepeat* CurrentSR = std::begin(StepPointers);
			uint16_t const _[] = { CurrentSR++->RepeatCount = Repeats... };
			for (;;) {
				PickRandomStep();
				if (!CurrentStep)
					return true;
				if (!CurrentStep->Restore(TrialsDone)) {
					TrialsDoneLeft = &TrialsDone;
					return false;
				}
			}
		}
		void Pause() const override {
			CurrentStep->Pause();
		}
		void Continue() const override {
			CurrentStep->Continue();
		}
		void Abort() const override {
			CurrentStep->Abort();
		}
		static void WriteInfoS(std::ostream& OutStream) {
			OutStream << WriteStructSize(2) << WriteStepID(UID::Step_Random) << static_cast<uint8_t>(UID::Property_Steps) << static_cast<uint8_t>(UID::Type_Array) << static_cast<uint8_t>(sizeof...(Steps));
			int _[] = { (OutStream << WriteStructSize(2) << static_cast<uint8_t>(UID::Property_Step), Steps::WriteInfoS(OutStream), OutStream << static_cast<uint8_t>(UID::Property_Repeat) << static_cast<uint8_t>(UID::Type_UInt16) << Repeats, 0)... };
		}
		void WriteInfoD(std::ostream& OutStream) const override {
			WriteInfoS(OutStream);
		}
		struct Repeatable {
			static_assert(false, "Random::WithRepeats不允许Repeatable");
		};

	protected:
		std::tuple<Steps...> StepsTuple;
		_StepWithRepeat StepPointers[sizeof...(Steps)];
		Step* CurrentStep;
		std::move_only_function<void() const> const ChildCallback;
		std::unordered_map<UID, uint16_t>* TrialsDoneLeft;
		void PickRandomStep() {
			uint16_t RepeatsLeft = 0;
			for (_StepWithRepeat& S : StepPointers)
				RepeatsLeft += S.RepeatCount;
			if (!RepeatsLeft) {
				CurrentStep = nullptr;
				return;
			}
			RepeatsLeft = random(0, RepeatsLeft);
			for (_StepWithRepeat& S : StepPointers)
				if (RepeatsLeft < S.RepeatCount) {
					S.RepeatCount--;
					CurrentStep = S.StepPointer;
					break;
				}
				else
					RepeatsLeft -= S.RepeatCount;
		}
	};

protected:
	std::unordered_map<UID, uint16_t>* TrialsDoneLeft;
};
template <typename... Steps>
using _Random_Selector = std::conditional_t<_Any<_ContainTrials<Steps>::value...>::value, _Random_WithTrials<Steps...>, _Random_Simple<Steps...>>;
// 按随机顺序执行步骤。支持::withRepeats扩展，以指定每个步骤的随机重复次数。
template <typename... Steps>
struct Random : _Random_Selector<Steps...> {
	using _Random_Selector<Steps...>::_Random_Selector;
};

// 将任意函数指针包装为步骤。可以额外指定一个自定义UID作为标识信息。
template <void (*Custom)(), UID FunctionID = UID::Step_CustomFunction>
struct FunctionToStep : Step {
	FunctionToStep(std::move_only_function<void() const> const&, Process const*) {}
	bool Start() override {
		Custom();
		return true;
	}
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << static_cast<uint8_t>(UID::Type_UID) << static_cast<uint8_t>(FunctionID);
	}
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}
	using Repeatable = FunctionToStep<Custom, FunctionID>;
};

// 执行一个Abortable步骤。如果该步骤被放弃，将转而执行WhenAbort步骤。此机制可用于执行自定义的步骤后清理工作。WhenAbort步骤不能暂停或放弃。
template <typename Abortable, typename WhenAbort>
struct DoWhenAborted : Abortable {
	WhenAbort WA;
	DoWhenAborted(std::move_only_function<void() const> const& ParentCallback, Process const* Container) : Abortable{ ParentCallback,Container }, WA{ ParentCallback,Container } {}
	void Abort() const override {
		Abortable::Abort();
		WA.Start();
	}
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(3) << WriteStepID(UID::Step_DoWhenAborted) << WriteStep(Abortable);
		OutStream << WriteStep(WhenAbort);
	}
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}
	using Repeatable = DoWhenAborted<Abortable, WhenAbort>;
};

// 在后台无限重复Repeatee，直到执行StopBackgroundRepeat为止。Repeatee的Start方法不能总是返回true，否则此步骤永不结束。如果Repeatee包含随机内容，每次都会重新随机抽取。
template <typename Repeatee, UID BackgroundID = UID::BackgroundID_Default>
struct StartBackgroundRepeat : Repeatee {
	static StartBackgroundRepeat<Repeatee, BackgroundID> const* RunningInstance;
	StartBackgroundRepeat(std::move_only_function<void() const> const&, Process const* Container) : Repeatee(ChildCallback, Container) {}
	bool Start() override {
		while (Repeatee::Start());
		RunningInstance = this;
		return true;
	}
	void Abort() const override {
		Repeatee::Abort();
		RunningInstance = nullptr;
	}
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(3) << WriteStepID(UID::Step_StartBackgroundRepeat) << WriteStep(Repeatee);
		OutStream << WriteField(BackgroundID);
	}
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}
	using Repeatable = StartBackgroundRepeat<Repeatee, BackgroundID>;
protected:
	std::move_only_function<void() const> const ChildCallback{ [this]() {
			while (Repeatee::Start());
		} };
};
template <typename Repeatee, UID BackgroundID>
StartBackgroundRepeat<Repeatee, BackgroundID> const* StartBackgroundRepeat<Repeatee, BackgroundID>::RunningInstance = nullptr;

template <typename Repeatee, UID BackgroundID = UID::BackgroundID_Default>
struct StopBackgroundRepeat : Step {
	StopBackgroundRepeat(std::move_only_function<void() const> const&, Process const*) {}
	bool Start() override {
		StartBackgroundRepeat<Repeatee, BackgroundID>::RunningInstance->Abort();
		return true;
	}
	using Repeatable = StopBackgroundRepeat<Repeatee, BackgroundID>;
	static void WriteInfoS(std::ostream& OutStream) {
		OutStream << WriteStructSize(3) << WriteStepID(UID::Step_StartBackgroundRepeat) << WriteStep(Repeatee);
		OutStream << WriteField(BackgroundID);
	}
	void WriteInfoD(std::ostream& OutStream) const override {
		WriteInfoS(OutStream);
	}
};
template <UID StepID, typename Step, typename = decltype(new Step)>
struct Pair {
	static constexpr UID ID = StepID;
	using StepType = Step;
};