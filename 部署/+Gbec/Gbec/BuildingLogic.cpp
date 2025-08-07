#include "UID.hpp"
#include <Quick_digital_IO_interrupt.hpp>
#include <TimersOneForAll_Declare.hpp>
#include <set>
#include <queue>
#include <unordered_map>
#include <sstream>
struct _PinListener
{
	uint8_t const Pin;
	std::move_only_function<void() const> const Callback;
	//中断不安全
	void Pause()const
	{
		std::set<std::move_only_function<void() const> const*>* CallbackSet = &Listening[Pin];
		CallbackSet->erase(&Callback);
		if (CallbackSet->empty())
			Quick_digital_IO_interrupt::DetachInterrupt(Pin);
		CallbackSet = &Resting[Pin];
		CallbackSet->erase(&Callback);
		if (CallbackSet->empty())
			Resting.erase(Pin);
	}
	//中断不安全
	void Continue()const
	{
		std::set<std::move_only_function<void() const> const*>& CallbackSet = Listening[Pin];
		if (CallbackSet.empty())
			Quick_digital_IO_interrupt::AttachInterrupt<RISING>(Pin, PinInterrupt{ Pin });
		CallbackSet.insert(&Callback);
	}
	//中断不安全
	_PinListener(uint8_t Pin, std::move_only_function<void() const>&& Callback)
		: Pin(Pin), Callback(std::move(Callback))
	{
		Continue();
	}
	//中断不安全
	~_PinListener()
	{
		Pause();
	}
	//中断安全
	static void ClearPending()
	{
		static std::queue<std::move_only_function<void() const> const*>LocalSwap;
		{
			Quick_digital_IO_interrupt::InterruptGuard const _;
			std::swap(LocalSwap, PendingCallbacks);
			for (auto& [Pin, Callbacks] : Resting)
			{
				std::set<std::move_only_function<void() const> const*>& ListeningSet = Listening[Pin];
				ListeningSet.merge(Callbacks);
				if (ListeningSet.size())
					Quick_digital_IO_interrupt::AttachInterrupt<RISING>(Pin, PinInterrupt{ Pin });
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
	static std::queue<std::move_only_function<void() const> const*> PendingCallbacks;
	static std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const*>> Listening;
	static std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const*>> Resting;
	struct PinInterrupt
	{
		uint8_t const Pin;
		void operator()() const
		{
			std::set<std::move_only_function<void() const> const*>& Listenings = Listening[Pin];
			for (auto H : Listenings)
				PendingCallbacks.push(H);
			Resting[Pin].merge(Listenings);
			Listenings.clear();
			Quick_digital_IO_interrupt::DetachInterrupt(Pin);
		}
	};
};
std::queue<std::move_only_function<void() const> const*> _PinListener::PendingCallbacks;
std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const*>> _PinListener::Listening;
std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const*>> _PinListener::Resting;

//中断安全
struct _ResourceManager
{
	void Pause()const
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		for (_PinListener const* H : ActiveInterrupts)
			H->Pause();
		for (Timers_one_for_all::TimerClass* T : ActiveTimers)
			T->Pause();
	}
	void Continue()const
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		for (_PinListener const* H : ActiveInterrupts)
			H->Continue();
		for (Timers_one_for_all::TimerClass* T : ActiveTimers)
			T->Continue();
	}
	void Abort()
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		_Abort();
		ActiveInterrupts.clear();
		ActiveTimers.clear();
	}
	virtual ~_ResourceManager()
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		_Abort();
	}
protected:
	std::set<_PinListener*> ActiveInterrupts;
	std::set<Timers_one_for_all::TimerClass*> ActiveTimers;
	_PinListener* RegisterInterrupt(uint8_t Pin, std::move_only_function<void() const>&& Callback)
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		_PinListener* Listener = new _PinListener(Pin, std::move(Callback));
		ActiveInterrupts.insert(Listener);
		return Listener;
	}
	void UnregisterInterrupt(_PinListener* Handle)
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		delete Handle;
		ActiveInterrupts.erase(Handle);
	}
	Timers_one_for_all::TimerClass* AllocateTimer()
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		Timers_one_for_all::TimerClass* Timer = Timers_one_for_all::AllocateTimer();
		ActiveTimers.insert(Timer);
		return Timer;
	}
	void UnregisterTimer(Timers_one_for_all::TimerClass* Timer)
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		ActiveTimers.erase(Timer);
		Timer->Allocatable = true;
	}
	//中断不安全
	void _Abort()
	{
		for (_PinListener* H : ActiveInterrupts)
			delete H;
		for (Timers_one_for_all::TimerClass* T : ActiveTimers)
		{
			T->Stop();
			T->Allocatable = true; // 使其可以被重新分配
		}
	}
};
inline static void InfoWrite(std::ostringstream& InfoStream, UID Info)
{
	InfoStream.put(static_cast<char>(Info));
}
inline static void InfoWrite(std::ostringstream& InfoStream, uint16_t Info)
{
	InfoStream.write(reinterpret_cast<const char*>(&Info), sizeof(Info));
}
template<typename...T>
inline static void InfoWrite(std::ostringstream& InfoStream, T...Info)
{
	int _[] = { (InfoWrite(InfoStream,Info),0)... };
}
// 同时开始所有子步骤
template <typename... Steps>
struct Meantime :_ResourceManager, virtual Steps...
{
	void Start()
	{
		int _[] = { (Steps::Start(), 0)... };
	}
	void Repeat(uint16_t Times)
	{
		int _[] = { (Steps::Repeat(Times), 0)... };
	}
	void Restore(std::unordered_map<UID, uint16_t>& TrialsDone)
	{
		int _[] = { (Steps::Restore(TrialsDone), 0)... };
	}
	static constexpr UID ID = UID::Step_Meantime;
	void WriteInfo(std::map<UID, std::string>Info)
	{
		if (!Info.contains(ID))
		{
			std::ostringstream InfoStream;
			InfoWrite(InfoStream, UID::Type_Array, sizeof...(Steps), UID::Type_UID, Steps::ID...);
			Info[ID] = InfoStream.str();
		}
	}
};