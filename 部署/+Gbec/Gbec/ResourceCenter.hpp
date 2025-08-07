#pragma once
#include <Quick_digital_IO_interrupt.hpp>
#include <set>
#include <functional>
extern std::set<std::move_only_function<void() const> const*> _PendingInterrupts;
template<uint8_t Pin>
struct _PinInterrupt {
	static std::set<std::move_only_function<void() const> const*> Handlers;
	static void AddHandler(std::move_only_function<void() const> const& Handler) {
		if (Handlers.empty())
			Quick_digital_IO_interrupt::AttachInterrupt<RISING>(Pin, []() {
				for (auto H : Handlers)
					_PendingInterrupts.insert(H);
			});
		Handlers.insert(&Handler);
	}
	static void RemoveHandler(std::move_only_function<void() const> const& Handler) {
		Handlers.erase(&Handler);
		if (Handlers.empty())
			Quick_digital_IO_interrupt::DetachInterrupt(Pin);
		_PendingInterrupts.erase(&Handler);
	}
};
template<uint8_t Pin>
std::set<std::move_only_function<void() const> const*> _PinInterrupt<Pin>::Handlers;