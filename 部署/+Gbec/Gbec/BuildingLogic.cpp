#include "UID.hpp"
#include "Quick_digital_IO_interrupt.hpp"
#include <set>
#include <queue>
#include <unordered_map>
struct PinListener
{
	uint8_t const Pin;
	std::move_only_function<void() const> const Callback;
	PinListener(uint8_t Pin, std::move_only_function<void() const>&& Callback)
		: Pin(Pin), Callback(std::move(Callback))
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		std::set<std::move_only_function<void() const> const*>& CallbackSet = Listening[Pin];
		CallbackSet.insert(&Callback);
		if (CallbackSet.size() == 1)
			Quick_digital_IO_interrupt::AttachInterrupt<RISING>(Pin, PinInterrupt{ Pin });
	}
	~PinListener()
	{
		Quick_digital_IO_interrupt::InterruptGuard const _;
		std::set<std::move_only_function<void() const> const*>* CallbackSet = &Listening[Pin];
		CallbackSet->erase(&Callback);
		if (CallbackSet->empty())
			Quick_digital_IO_interrupt::DetachInterrupt(Pin);
		CallbackSet = &Resting[Pin];
		CallbackSet->erase(&Callback);
		if (CallbackSet->empty())
			Resting.erase(Pin);
	}
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
struct ResourceManager
{
protected:
	std::s
};
// 同时开始所有子步骤
template <typename... Steps>
struct Meantime : virtual Steps...
{
	void Start()
	{
		int _[] = { (Steps::Start(), 0)... };
	}
};