#pragma once
#include "UID.hpp"
#include "Timers_one_for_all.hpp"
#include <Quick_digital_IO_interrupt.hpp>
#include <chrono>
struct AutomaticallyEndedTest
{
	virtual void Run(uint16_t RepeatTimes) = 0;
};
struct Step
{
	// 返回true表示步骤已完成可以进入下一步，返回false表示步骤未完成需要等待FinishCallback
	virtual bool Start(std::move_only_function<void() const> const *FinishCallback) = 0;
	virtual void Pause() {}
	virtual void Continue() {}
	virtual void Abort() {}
	virtual bool Restore(std::unordered_map<UID, uint16_t> &TrialsDone, std::move_only_function<void() const> const *FinishCallback) {}
	virtual ~Step() {}
};
template <uint8_t Pin, bool HighOrLow>
struct DigitalWrite : Step
{
	bool Start(std::move_only_function<void() const> const *FinishCallback) override
	{
		Quick_digital_IO_interrupt::DigitalWrite<Pin, HighOrLow>();
		return true;
	}
};
template <uint16_t Count>
struct Milliseconds
{
	static constexpr std::chrono::milliseconds Value = std::chrono::milliseconds(Count);
};
template <typename T>
struct Delay : Step
{
	Timers_one_for_all::TimerClass const *Timer;
	bool Start(std::move_only_function<void() const> const *FinishCallback) override
	{
		Timer = Timers_one_for_all::AllocateTimer();
		Timer->DoAfter(T::Value, [Timer, FinishCallback]()
					   {
			FinishCallback->operator()();
			Timer->Allocatable(true); });
	}
};
#define UidPairClass(Uid, Class) {UID::Uid, []() { return new Class; }}