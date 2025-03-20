#pragma once
#include "UID.hpp"
#include "Timers_one_for_all.hpp"
#include <Quick_digital_IO_interrupt.hpp>
#include <chrono>
#include <unordered_map>
#include <ostream>
#include <set>
#include <map>
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
		Repeat();
		return true;
	}
	// 类似于Start，但必须重复和上次Start或Repeat完全相同的行为。这仅对于那些存在随机性的步骤有意义，对于确定性步骤此方法除了没有返回值以外应当与Start做完全相同的事情。
	virtual void Repeat() {}
	// 暂停当前步骤。不能暂停的步骤可以不override此方法。
	virtual void Pause() const {}
	// 继续已暂停的步骤。不能暂停的步骤可以不override此方法。
	virtual void Continue() const {}
	// 放弃当前步骤。不能放弃的步骤可以不override此方法。
	virtual void Abort() const {}
	// 从断线重连中恢复。不能恢复的步骤可以不override此方法。返回值的意义和Start相同
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
template <uint8_t Pin, bool HighOrLow>
struct DigitalWrite : Step
{
	DigitalWrite(std::move_only_function<void() const> const &)
	{
		Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
	}
	void Repeat() override
	{
		Quick_digital_IO_interrupt::DigitalWrite<Pin, HighOrLow>();
	}
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << WriteStructSize(3) << WriteStepID(UID::Step_DigitalWrite) << WriteField(Pin) << WriteField(HighOrLow);
	}
};
template <uint16_t Count>
struct Milliseconds
{
	static constexpr std::chrono::milliseconds Value = std::chrono::milliseconds(Count);
	static constexpr UID Type = UID::Type_Milliseconds;
};
template <uint8_t Count>
struct Seconds
{
	static constexpr std::chrono::seconds Value = std::chrono::seconds(Count);
	static constexpr UID Type = UID::Type_Seconds;
};
#define WriteDuration(FieldName, Duration) static_cast<uint8_t>(UID::Property_##FieldName) << static_cast<uint8_t>(Duration::Type) << Duration::Value.count()
template <typename Duration>
struct Delay : Step
{
	std::move_only_function<void() const> const &FinishCallback;
	Delay(std::move_only_function<void() const> const &FinishCallback) : FinishCallback(FinishCallback) {}
	Timers_one_for_all::TimerClass *Timer;
	void Repeat() override
	{
		Timer = Timers_one_for_all::AllocateTimer();
		Timer->DoAfter(Duration::Value, [this]()
					   {
				Timer->Allocatable = true;
				FinishCallback(); });
	}
	bool Start() override
	{
		Repeat();
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

protected:
	Timers_one_for_all::TimerClass *const Timer = Timers_one_for_all::AllocateTimer();
};
//不能使用指针，因为执行时会交换到本地，如果在中断时释放资源，本地副本将会执行到空指针。为防止此情况
extern std::map<uint8_t, std::move_only_function<void() const>> PinMonitors;
extern std::move_only_function<void() const> const NullCallback;
#define WriteStep(FieldName)                         \
	static_cast<uint8_t>(UID::Property_##FieldName); \
	FieldName::WriteInfo(OutStream);
template <uint8_t Pin, typename Reporter>
struct StartMonitor : Step
{
	StartMonitor(std::move_only_function<void() const> const &)
	{
		Quick_digital_IO_interrupt::PinMode<Pin, INPUT>();
	}
	void Repeat() override
	{
		PinMonitors[Pin] = [R = Reporter(NullCallback)]()
		{ R.Start(); };
	}
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << WriteStructSize(3) << WriteStepID(UID::Step_StartMonitor) << WriteField(Pin) << WriteStep(Reporter);
	}
};
template <uint8_t Pin>
struct StopMonitor : Step
{
	StopMonitor(std::move_only_function<void() const> const &) {}
	void Repeat() override
	{
		PinMonitors.erase(Pin);
	}
	static void WriteInfo(std::ostream &OutStream)
	{
		OutStream << WriteStructSize(2) << WriteStepID(UID::Step_StopMonitor) << WriteField(Pin);
	}
};
template <typename RepeateeType, uint8_t Pin>
struct RepeatIfPin
{
	RepeateeType Repeatee;
	RepeatIfPin(std::move_only_function<void() const> const &FinishCallback) : Repeatee(RepeateeType(FinishCallback)) {}
	bool RepeateeStartReturn;
	bool Start() override
	{
		if (RepeateeStartReturn = Repeatee.Start())
			return true;
		PinMonitors[Pin] = [&Repeatee]()
		{
			Repeatee.Abort();
			Repeatee.Repeat();
		};
		return false;
	}
	void Repeat() override
	{
		Repeatee.Repeat();
		if (!RepeateeStartReturn)
			PinMonitors[Pin] = [&Repeatee]()
			{
				Repeatee.Abort();
				Repeatee.Repeat();
			};
	}
	void Pause()const override
	{
		Repeatee.Pause();
		
	}
};
#define UidPairClass(Uid, Class) {UID::Uid, []() { return new Class; }}