#pragma once
#include "UID.hpp"
#include "Timers_one_for_all.hpp"
#include <Quick_digital_IO_interrupt.hpp>
#include <chrono>
#include <unordered_map>
struct AutomaticallyEndedTest
{
	virtual void Run(uint16_t RepeatTimes) = 0;
};
// 通用步骤接口。实际步骤不一定实现所有方法。
struct Step
{
	// 返回true表示步骤已完成可以进入下一步，返回false表示步骤未完成需要等待FinishCallback
	virtual bool Start(std::move_only_function<void() const> const *FinishCallback) const
	{
		return true;
	}
	virtual void Pause() const {}
	virtual void Continue() const {}
	virtual void Abort() const {}
	virtual bool Restore(std::unordered_map<UID, uint16_t> &TrialsDone, std::move_only_function<void() const> const *FinishCallback) const
	{
		return true;
	}
	virtual bool WriteInfo(std::ostream &OutStream) const {}
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
template <uint8_t Count>
struct Seconds
{
	static constexpr std::chrono::seconds Value = std::chrono::seconds(Count);
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
				Timer->Allocatable = true;
				FinishCallback->operator()(); });
		return false
	}
};
#define UidPairClass(Uid, Class) {UID::Uid, []() { return new Class; }}