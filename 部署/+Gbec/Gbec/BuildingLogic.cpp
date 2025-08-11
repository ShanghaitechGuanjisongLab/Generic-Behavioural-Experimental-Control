#include "UID.hpp"
#include <Quick_digital_IO_interrupt.hpp>
#include <TimersOneForAll_Declare.hpp>
#include <set>
#include <queue>
#include <unordered_map>
#include <sstream>
#include <chrono>
#include <map>
#include <random>
struct PinListener
{
	uint8_t const Pin;
	std::move_only_function<void() const> const Callback;
	// 中断不安全
	void Pause() const
	{
		std::set<std::move_only_function<void() const> const *> *CallbackSet = &Listening[Pin];
		CallbackSet->erase(&Callback);
		if (CallbackSet->empty())
			Quick_digital_IO_interrupt::DetachInterrupt(Pin);
		CallbackSet = &Resting[Pin];
		CallbackSet->erase(&Callback);
		if (CallbackSet->empty())
			Resting.erase(Pin);
	}
	// 中断不安全
	void Continue() const
	{
		std::set<std::move_only_function<void() const> const *> &CallbackSet = Listening[Pin];
		if (CallbackSet.empty())
			Quick_digital_IO_interrupt::AttachInterrupt<RISING>(Pin, PinInterrupt{Pin});
		CallbackSet.insert(&Callback);
	}
	// 中断不安全
	PinListener(uint8_t Pin, std::move_only_function<void() const> &&Callback)
		: Pin(Pin), Callback(std::move(Callback))
	{
		Continue();
	}
	// 中断不安全
	~PinListener()
	{
		Pause();
	}
	// 中断安全
	static void ClearPending()
	{
		static std::queue<std::move_only_function<void() const> const *> LocalSwap;
		{
			Quick_digital_IO_interrupt::InterruptGuard const _;
			std::swap(LocalSwap, PendingCallbacks);
			for (auto &[Pin, Callbacks] : Resting)
			{
				std::set<std::move_only_function<void() const> const *> &ListeningSet = Listening[Pin];
				ListeningSet.merge(Callbacks);
				if (ListeningSet.size())
					Quick_digital_IO_interrupt::AttachInterrupt<RISING>(Pin, PinInterrupt{Pin});
				Callbacks.clear();
			}
		}
		while (LocalSwap.size())
		{
			LocalSwap.front()->operator()();
			LocalSwap.pop();
		}
	}

protected:
	static std::queue<std::move_only_function<void() const> const *> PendingCallbacks;
	static std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const *>> Listening;
	static std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const *>> Resting;
	struct PinInterrupt
	{
		uint8_t const Pin;
		void operator()() const
		{
			std::set<std::move_only_function<void() const> const *> &Listenings = Listening[Pin];
			for (auto H : Listenings)
				PendingCallbacks.push(H);
			Resting[Pin].merge(Listenings);
			Listenings.clear();
			Quick_digital_IO_interrupt::DetachInterrupt(Pin);
		}
	};
};
std::queue<std::move_only_function<void() const> const *> PinListener::PendingCallbacks;
std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const *>> PinListener::Listening;
std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const *>> PinListener::Resting;

inline static void InfoWrite(std::ostringstream &InfoStream, UID InfoValue)
{
	InfoStream.put(static_cast<char>(InfoValue));
}
inline static void InfoWrite(std::ostringstream &InfoStream, uint16_t InfoValue)
{
	InfoStream.write(reinterpret_cast<const char *>(&InfoValue), sizeof(InfoValue));
}
using InfoKey = UID const *;
inline static void InfoWrite(std::ostringstream &InfoStream, InfoKey InfoValue)
{
	InfoStream.write(reinterpret_cast<const char *>(&InfoValue), sizeof(InfoValue));
}
template <typename... T>
inline static void InfoWrite(std::ostringstream &InfoStream, T... InfoValues)
{
	int _[] = {(InfoWrite(InfoStream, InfoValues), 0)...};
}

// 中断安全
struct Step
{
	void Pause() const
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		for (PinListener const *H : ActiveInterrupts)
			H->Pause();
		for (Timers_one_for_all::TimerClass *T : ActiveTimers)
			T->Pause();
	}
	void Continue() const
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		for (PinListener const *H : ActiveInterrupts)
			H->Continue();
		for (Timers_one_for_all::TimerClass *T : ActiveTimers)
			T->Continue();
	}
	void GlobalAbort()
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		_Abort();
		ActiveInterrupts.clear();
		ActiveTimers.clear();
		TrialsDone.clear();
	}
	virtual ~Step()
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		_Abort();
	}
	// 返回是否需要等待回调。回调函数只能在Start时提供，不能在构造时提供，因为存在复杂的虚继承关系，只能默认构造。
	virtual bool Start(void (*)(Step *))
	{
		return false;
	}
	virtual void WriteInfo(std::map<InfoKey, std::string> &InfoMap) const = 0;

protected:
	std::set<PinListener *> ActiveInterrupts;
	std::set<Timers_one_for_all::TimerClass *> ActiveTimers;
	std::unordered_map<UID, uint16_t> TrialsDone;
	PinListener *RegisterInterrupt(uint8_t Pin, std::move_only_function<void() const> &&Callback)
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		PinListener *Listener = new PinListener(Pin, std::move(Callback));
		ActiveInterrupts.insert(Listener);
		return Listener;
	}
	void UnregisterInterrupt(PinListener *Handle)
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		delete Handle;
		ActiveInterrupts.erase(Handle);
	}
	Timers_one_for_all::TimerClass *AllocateTimer()
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		Timers_one_for_all::TimerClass *Timer = Timers_one_for_all::AllocateTimer();
		ActiveTimers.insert(Timer);
		return Timer;
	}
	// 此操作不负责停止计时器，仅使其可以被再分配
	void UnregisterTimer(Timers_one_for_all::TimerClass *Timer)
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		ActiveTimers.erase(Timer);
		Timer->Allocatable = true;
	}
	// 中断不安全
	void _Abort()
	{
		for (PinListener *H : ActiveInterrupts)
			delete H;
		for (Timers_one_for_all::TimerClass *T : ActiveTimers)
		{
			T->Stop();
			T->Allocatable = true; // 使其可以被重新分配
		}
	}
	//提前终止当前执行中的步骤，直接跳到下一步
	virtual void Skip() {}
	//重新开始当前执行中的步骤，不改变下一步
	virtual void Restart() {}
};
// 顺序执行所有子步骤
template <typename... SubSteps>
struct Sequential : virtual SubSteps...
{
	static void NextBlock(Step *Me)
	{
		Sequential *TypedMe = reinterpret_cast<Sequential *>(Me);
		while (++TypedMe->CurrentStart < std::end(TypedMe->SubStarts))
			if ((*TypedMe->CurrentStart)(TypedMe, NextBlock))
				return;
		FinishCallback(TypedMe);
	}
	bool Start(void (*FC)(Step *)) override
	{
		for (CurrentStart = std::begin(SubStarts); CurrentStart < std::end(SubStarts); ++CurrentStart)
			if ((*CurrentStart)(this, NextBlock))
			{
				FinishCallback = FC;
				return true;
			}
		return false;
	}
	void Restart()
	{
		Start(FinishCallback);
	}
	// 标准保证不同模板实例的该对象有不同的地址
	static constexpr UID ID = UID::Step_Sequential;
	void WriteInfo(std::map<InfoKey, std::string> &InfoMap) const override
	{
		if (!InfoMap.contains(&ID))
		{
			std::ostringstream InfoStream;
			InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_SubSteps, UID::Type_Array, sizeof...(SubSteps), UID::Type_Pointer, &SubSteps::ID...);
			InfoMap[&ID] = InfoStream.str();
			int _[] = {(SubSteps::WriteInfo(InfoMap), 0)...};
		}
	}

protected:
	static constexpr bool (*SubStarts[])(Sequential *, void (*)(Step *)) = {[](Sequential *Me, void (*FinishCallback)(Step *))
																			{ return Me->SubSteps::Start(FinishCallback); }...};
	bool (*const *CurrentStart)(Sequential *, void (*)(Step *));
	void (*FinishCallback)(Step *) = [](Step) {}
};
static constexpr
#ifdef ARDUINO_ARCH_AVR
	std::ArduinoUrng
#endif
#ifdef ARDUINO_ARCH_SAM
		std::TrueUrng
#endif
			Urng;
// 随机执行所有子步骤。多次执行顺序不会改变，必须先执行Randomize步骤才能重新随机化
template <typename... SubSteps>
struct RandomSequential : virtual SubSteps...
{
	static void NextBlock(Step *Me)
	{
		RandomSequential *TypedMe = reinterpret_cast<RandomSequential *>(Me);
		while (++TypedMe->CurrentStart < std::end(TypedMe->SubStarts))
			if ((*TypedMe->CurrentStart)(TypedMe, NextBlock))
				return;
		FinishCallback(TypedMe);
	}
	bool Start(void (*FC)(Step *)) override
	{
		for (CurrentStart = std::begin(SubStarts); CurrentStart < std::end(SubStarts); ++CurrentStart)
			if ((*CurrentStart)(this, NextBlock))
			{
				FinishCallback = FC;
				return true;
			}
		return false;
	}
	static constexpr UID ID = UID::Step_RandomSequential;
	void WriteInfo(std::map<InfoKey, std::string> &InfoMap) const override
	{
		if (!InfoMap.contains(&ID))
		{
			std::ostringstream InfoStream;
			InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_SubSteps, UID::Type_Array, sizeof...(SubSteps), UID::Type_Pointer, &SubSteps::ID...);
			InfoMap[&ID] = InfoStream.str();
			int _[] = {(SubSteps::WriteInfo(InfoMap), 0)...};
		}
	}
	void Randomize()
	{
		std::shuffle(std::begin(SubStarts), std::end(SubStarts), Urng);
	}
	RandomSequential()
	{
		Randomize();
	}
	// 必须为每个子步骤指定一个重复次数
	template <uint16_t... Repeats>
	struct WithRepeats : virtual SubSteps...
	{
		static void NextBlock(Step *Me)
		{
			WithRepeats *TypedMe = reinterpret_cast<WithRepeats *>(Me);
			while (++TypedMe->CurrentStart < std::end(TypedMe->SubStarts))
				if ((*TypedMe->CurrentStart)(TypedMe, NextBlock))
					return;
			FinishCallback(TypedMe);
		}
		bool Start(void (*FC)(Step *)) override
		{
			for (CurrentStart = std::begin(SubStarts); CurrentStart < std::end(SubStarts); ++CurrentStart)
				if ((*CurrentStart)(this, NextBlock))
				{
					FinishCallback = FC;
					return true;
				}
			return false;
		}
		static constexpr UID ID = UID::Step_RandomSequential;
		void WriteInfo(std::map<InfoKey, std::string> &InfoMap) const override
		{
			if (!InfoMap.contains(&ID))
			{
				std::ostringstream InfoStream;
				InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_SubSteps, UID::Type_Table, sizeof...(SubSteps), 2, UID::Column_Module, UID::Type_Pointer, &SubModules::ID..., UID::Column_Repeats, UID::Type_UInt16, Repeats...);
				InfoMap[&ID] = InfoStream.str();
				int _[] = {(SubSteps::WriteInfo(InfoMap), 0)...};
			}
		}
		void Randomize()
		{
			std::shuffle(std::begin(SubStarts), std::end(SubStarts), Urng);
		}
		WithRepeats()
		{
			auto _[] = {CurrentStart = std::fill_n(CurrentStart, Repeats, [](WithRepeats *Me, void (*FinishCallback)(Step *))
												   { return Me->SubSteps::Start(FinishCallback); })...};
			Randomize();
		}
		void Restart()
		{
			Start(FinishCallback);
		}

	protected:
		template <uint16_t First, uint16_t... Rest>
		struct Sum
		{
			static constexpr uint16_t value = First + Sum<Rest...>::value;
		};
		template <uint16_t Only>
		struct Sum<Only>
		{
			static constexpr uint16_t value = Only;
		};
		bool (*SubStarts[Sum<Repeats>::value])(WithRepeats *, void (*)(Step *));
		bool (**CurrentStart)(WithRepeats *, void (*)(Step *)) = std::begin(SubStarts);
		void (*FinishCallback)(Step *) = [](Step *) {};
	};
	void Restart()
	{
		Start(FinishCallback);
	}

protected:
	bool (*SubStarts[])(RandomSequential *, void (*)(Step *)) = {[](RandomSequential *self, void (*FC)(Step *))
																 { return self->SubSteps::Start(FC); }...};
	bool (*const *CurrentStart)(RandomSequential *, void (*)(Step *)) = std::begin(SubStarts);
	void (*FinishCallback)(Step *) = [](Step *) {};
};

template <typename T>
struct _TypeID;
template <>
struct _TypeID<std::chrono::seconds>
{
	static constexpr UID value = UID::Type_Seconds;
};
template <>
struct _TypeID<std::chrono::milliseconds>
{
	static constexpr UID value = UID::Type_Milliseconds;
};
template <typename Unit, uint16_t Value>
struct ConstantDuration
{
	template <typename GetUnit>
	static constexpr GetUnit Get()
	{
		return std::chrono::duration_cast<GetUnit>(Unit{Value});
	}
	static void WriteInfo(std::map<InfoKey, std::string> &, std::ostringstream &InfoStream)
	{
		InfoWrite(InfoStream, _TypeID<Unit>::value, Value);
	}
};
// 表示一个随机时间段。Unit必须是std::chrono::duration的一个实例。该步骤维护一个随机变量，只有使用Randomize步骤才能改变这个变量，否则一直保持相同的值。如果有多个随机范围相同的随机变量需要独立控制随机性，可以指定不同的ID，否则视为同一个随机变量。
template <typename Unit, uint16_t Min, uint16_t Max, UID CustomID = UID::Duration_Random>
struct RandomDuration
{
	Unit Current;
	void Randomize()
	{
		constexpr double DoubleMin = Min;
		Current = Unit{pow(static_cast<double>(Max) / DoubleMin, static_cast<double>(random(__LONG_MAX__)) / (__LONG_MAX__ - 1)) * DoubleMin};
	}
	template <typename GetUnit>
	GetUnit Get() const
	{
		return std::chrono::duration_cast<GetUnit>(Current);
	}
	RandomDuration()
	{
		Randomize();
	}
	static void WriteInfo(std::map<InfoKey, std::string> &InfoMap, std::ostringstream &InfoStream)
	{
		static constexpr UID ID = CustomID;
		if (!InfoMap.contains(&ID))
		{
			std::ostringstream MyStream;
			InfoWrite(MyStream, UID::Type_Array, 2, _TypeID<Unit>::value, Min, Max);
			InfoMap[&ID] = MyStream.str();
		}
		InfoWrite(InfoStream, UID::Type_Pointer, &ID);
	}
};
struct InfiniteDuration;
// 令具有随机性的步骤或时间执行随机化
template <typename RandomStep>
struct Randomize : virtual RandomStep
{
	bool Start(void (*)(Step *)) override
	{
		if (TrialsDone.empty())
			RandomStep::Randomize();
		return false;
	}
	static constexpr UID ID = UID::Step_Randomize;
	void WriteInfo(std::map<InfoKey, std::string> &InfoMap) const override
	{
		if (!InfoMap.contains(&ID))
		{
			std::ostringstream InfoStream;
			InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_RandomStep, UID::Type_Pointer, &RandomStep::ID);
			InfoMap[&ID] = InfoStream.str();
			RandomStep::WriteInfo(InfoMap);
		}
	}
};
template <typename Unit, uint16_t Min, uint16_t Max, UID ID>
struct Randomize<RandomDuration<Unit, Min, Max, ID>> : virtual RandomDuration<Unit, Min, Max, ID>, virtual Step
{
	bool Start(void (*)(Step *)) override
	{
		if (TrialsDone.empty())
			RandomDuration<Unit, Min, Max, ID>::Randomize();
		return false;
	}
	static constexpr UID ID = UID::Step_Randomize;
	void WriteInfo(std::map<InfoKey, std::string> &InfoMap) const override
	{
		if (!InfoMap.contains(&ID))
		{
			std::ostringstream InfoStream;
			InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_RandomDuration);
			RandomDuration<Unit, Min, Max, ID>::WriteInfo(InfoMap, InfoStream);
			InfoMap[&ID] = InfoStream.str();
		}
	}
};
template <typename Duration, UID CustomID = UID::Step_Delay>
struct Delay : virtual Step, virtual Duration
{
	void (*FinishCallback)(Step *);
	void Abort()
	{
		Timer->Stop();
		_Abort();
	}
	bool Start(void (*FC)(Step *)) override
	{
		if (TrialsDone.empty())
		{
			FinishCallback = FC;
			Timer = AllocateTimer();
			Timer->DoAfter(Duration::Get<Tick>(), [this]()
						   { _Abort(); });
			return true;
		}
		return false;
	}
	static constexpr UID ID = CustomID;
	void WriteInfo(std::map<InfoKey, std::string> &InfoMap) const override
	{
		if (!InfoMap.contains(&ID))
		{
			std::ostringstream InfoStream;
			InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Duration);
			Duration::WriteInfo(InfoMap, InfoStream);
			InfoMap[&ID] = InfoStream.str();
		}
	}

protected:
	Timers_one_for_all::TimerClass *Timer;
	void _Abort()
	{
		UnregisterTimer(Timer);
		FinishCallback(this);
	}
};
// 无限等待，永不返回，但可以Abort
template <UID CustomID>
struct Delay<InfiniteDuration, CustomID> : virtual Step
{
	void (*FinishCallback)(Step *);
	bool Start(void (*FC)(Step *)) override
	{
		if (TrialsDone.empty())
		{
			FinishCallback = FC;
			return true;
		}
		return false;
	}
	void Abort()
	{
		FinishCallback(this);
	}
	static constexpr UID ID = CustomID;
	void WriteInfo(std::map<InfoKey, std::string> &InfoMap) const override
	{
		if (!InfoMap.contains(&ID))
		{
			std::ostringstream InfoStream;
			InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Duration, UID::Type_UID, UID::Duration_Infinite);
			InfoMap[&ID] = InfoStream.str();
		}
	}
};
template <typename AbortStep>
struct Abort : virtual AbortStep
{
	bool Start(void (*FC)(Step *)) override
	{
		if (TrialsDone.empty())
			AbortStep::Abort();
		return false;
	}
	static constexpr UID ID = UID::Step_Abort;
	void WriteInfo(std::map<InfoKey, std::string> &InfoMap) const override
	{
		if (!InfoMap.contains(&ID))
		{
			std::ostringstream InfoStream;
			InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID,UID::Field_Pin,UID::Type_UInt8, UID::Field_AbortStep, UID::Type_Pointer, &AbortStep::ID);
			InfoMap[&ID] = InfoStream.str();
			AbortStep::WriteInfo(InfoMap);
		}
	}
};
