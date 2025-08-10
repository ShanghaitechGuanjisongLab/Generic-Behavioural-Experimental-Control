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

inline static void InfoWrite(std::ostringstream &InfoStream, UID InfoMap)
{
	InfoStream.put(static_cast<char>(InfoMap));
}
inline static void InfoWrite(std::ostringstream &InfoStream, uint16_t InfoMap)
{
	InfoStream.write(reinterpret_cast<const char *>(&InfoMap), sizeof(InfoMap));
}
template <typename... T>
inline static void InfoWrite(std::ostringstream &InfoStream, T... InfoMap)
{
	int _[] = {(InfoWrite(InfoStream, InfoMap), 0)...};
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
	void Abort()
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
	// 返回是否需要等待回调
	virtual bool Start()
	{
		return false;
	}
	virtual void WriteInfo(std::map<UID, std::string> &InfoMap) const = 0;

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
	template <typename... T>
	static void _WriteInfo(UID ID, std::map<UID, std::string> &InfoMap, T... InfoValues)
	{
		if (!InfoMap.contains(ID))
		{
			std::ostringstream InfoStream;
			InfoWrite(InfoStream, InfoValues...);
			InfoMap[ID] = InfoStream.str();
		}
	}
};
// 顺序执行所有子步骤
template <typename... SubSteps>
struct Sequential : virtual SubSteps...
{
	void NextBlock()
	{
		while (++CurrentStart < std::end(SubStarts))
			if ((*CurrentStart)(this))
				return;
		FinishCallback(this);
	}
	bool Start() override
	{
		for (CurrentStart = std::begin(SubStarts); CurrentStart < std::end(SubStarts); ++CurrentStart)
			if ((*CurrentStart)(this))
				return true;
		return false;
	}
	static constexpr UID ID = UID::Step_Sequential;
	void WriteInfo(std::map<UID, std::string> &InfoMap) const override
	{
		if (!InfoMap.contains(ID))
		{
			std::ostringstream InfoStream;
			InfoWrite(InfoStream, UID::Type_Array, sizeof...(SubSteps), UID::Type_UID, SubSteps::ID...);
			InfoMap[ID] = InfoStream.str();
			int _[] = {(SubSteps::WriteInfo(InfoMap), 0)...};
		}
	}
	Sequential(std::move_only_function<void(Step *) const> &&FinishCallback) : SubSteps([](Step *Me)
																						{ reinterpret_cast<Sequential *>(Me)->NextBlock(); })...,
																			   FinishCallback(std::move(FinishCallback)) {}

protected:
	static constexpr bool (*SubStarts[])(Sequential *) = {[](Sequential *self)
														  { return self->SubSteps::Start(); }...};
	bool (*const *CurrentStart)(Sequential *);
	std::move_only_function<void(Step *) const> const FinishCallback;
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
	void NextBlock()
	{
		while (++CurrentStart < std::end(SubStarts))
			if ((*CurrentStart)(this))
				return;
		FinishCallback(this);
	}
	bool Start() override
	{
		for (CurrentStart = std::begin(SubStarts); CurrentStart < std::end(SubStarts); ++CurrentStart)
			if ((*CurrentStart)(this))
				return true;
		return false;
	}
	static constexpr UID ID = UID::Step_RandomSequential;
	void WriteInfo(std::map<UID, std::string> &InfoMap) const override
	{
		if (!InfoMap.contains(ID))
		{
			std::ostringstream InfoStream;
			InfoWrite(InfoStream, UID::Type_Array, sizeof...(SubSteps), UID::Type_UID, SubSteps::ID...);
			InfoMap[ID] = InfoStream.str();
			int _[] = {(SubSteps::WriteInfo(InfoMap), 0)...};
		}
	}
	void Randomize()
	{
		std::shuffle(std::begin(SubStarts), std::end(SubStarts), Urng);
	}
	RandomSequential(std::move_only_function<void(Step *) const> &&FinishCallback) : SubSteps([](Step *Me)
																							  { reinterpret_cast<Sequential *>(Me)->NextBlock(); })...,
																					 FinishCallback(std::move(FinishCallback))
	{
		Randomize();
	}
	// 必须为每个子步骤指定一个重复次数
	template <uint16_t... Repeats>
	struct WithRepeats : virtual SubSteps...
	{
		void NextBlock()
		{
			while (++CurrentStart < std::end(SubStarts))
				if ((*CurrentStart)(this))
					return;
			FinishCallback(this);
		}
		bool Start() override
		{
			for (CurrentStart = std::begin(SubStarts); CurrentStart < std::end(SubStarts); ++CurrentStart)
				if ((*CurrentStart)(this))
					return true;
			return false;
		}
		static constexpr UID ID = UID::Step_RandomSequential;
		void WriteInfo(std::map<UID, std::string> &InfoMap) const override
		{
			if (!InfoMap.contains(ID))
			{
				std::ostringstream InfoStream;
				InfoWrite(InfoStream, UID::Type_Table, sizeof...(SubSteps), 2, UID::Column_SubSteps, UID::Type_UID, SubSteps::ID..., UID::Column_Repeats, UID::Type_UInt16, Repeats...);
				InfoMap[ID] = InfoStream.str();
				int _[] = {(SubSteps::WriteInfo(InfoMap), 0)...};
			}
		}
		void Randomize()
		{
			std::shuffle(std::begin(SubStarts), std::end(SubStarts), Urng);
		}
		WithRepeats(std::move_only_function<void(Step *) const> &&FinishCallback) : SubSteps([](Step *Me)
																							 { reinterpret_cast<Sequential *>(Me)->NextBlock(); })...,
																					FinishCallback(std::move(FinishCallback))
		{
			auto _[] = {CurrentStart = std::fill_n(CurrentStart, Repeats, [](WithRepeats *Me)
												   { return Me->SubSteps::Start(); })...};
			Randomize();
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
		bool (*SubStarts[Sum<Repeats>::value])(WithRepeats *);
		bool (**CurrentStart)(WithRepeats *) = std::begin(SubStarts);
		std::move_only_function<void(Step *) const> const FinishCallback;
}

protected : bool (*SubStarts[])(RandomSequential *) = {[](RandomSequential *self)
													   { return self->SubSteps::Start(); }...};
	bool (*const *CurrentStart)(RandomSequential *);
	std::move_only_function<void(Step *) const> const FinishCallback;
};

// 表示一个常数时间段。Unit必须是std::chrono::duration的一个实例。
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
struct Duration
{
	template <typename GetUnit>
	static constexpr GetUnit Get()
	{
		return std::chrono::duration_cast<GetUnit>(Unit{Value});
	}
	static void WriteInfo(std::map<UID, std::string> &, std::ostringstream &InfoStream)
	{
		InfoWrite(InfoStream, _TypeID<Unit>::value, Value);
	}
};
// 表示一个随机时间段。Unit必须是std::chrono::duration的一个实例。该步骤维护一个随机变量，只有使用Randomize步骤才能改变这个变量，否则一直保持相同的值。必须指定一个独特的ID，以区分不同的随机变量。
template <typename Unit, uint16_t Min, uint16_t Max, UID ID>
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
	static void WriteInfo(std::map<UID, std::string> &InfoMap, std::ostringstream &InfoStream)
	{
		if (!InfoMap.contains(ID))
		{
			std::ostringstream MyStream;
			InfoWrite(MyStream, UID::Type_Array, 2, _TypeID<Unit>::value, Min, Max);
			InfoMap[ID] = MyStream.str();
		}
		InfoWrite(InfoStream, UID::Type_UID, ID);
	}
};
// 令具有随机性的步骤执行随机化
template <typename RD>
struct Randomize : virtual RD
{
	void Start() override
	{
		RD::Randomize();
	}
	static constexpr UID ID = UID::Step_Randomize;
	void WriteInfo(std::map<UID, std::string> &InfoMap) const override
	{
		if (!InfoMap.contains(ID))
		{
			std::ostringstream InfoStream;
			RD::WriteInfo(InfoMap, InfoStream);
			InfoMap[ID] = InfoStream.str();
		}
	}
};
template <typename After, typename Do>
struct DoAfter : virtual After, virtual Do
{
	void Start() override
	{
		Timers_one_for_all::TimerClass *Timer = Timers_one_for_all::AllocateTimer();
		Timer->DoAfter(After::Get(), [this, Timer]()
					   {
			Step::UnregisterTimer(Timer);
			Do::Start(); });
	}
	static constexpr UID ID = UID::Step_DoWhenAborted;
	void WriteInfo(std::map<UID, std::string> &InfoMap) const override
	{
		if (!InfoMap.contains(ID))
		{
			std::ostringstream InfoStream;
			InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_After);
			After::WriteInfo(InfoMap, InfoStream);
			InfoWrite(InfoStream, UID::Field_Do, Do::ID);
			InfoMap[ID] = InfoStream.str();
			Do::WriteInfo(InfoMap);
		}
	}
};