#include "Async_stream_IO.hpp"
#include <queue>
#include <unordered_map>
#include <set>

// 访问全局容器时必须禁用中断，调用move_only_function前必须启用中断。

static std::unordered_map<uint8_t, std::move_only_function<void() const>> Listeners;
static std::set<Stream*> StreamsInvolved;

// 调用这些方法前必须禁用中断

static uint8_t AllocatePort() {
	static uint8_t Port = 0;
	while (Listeners.contains(Port))
		++Port;
	return Port;
}
template <bool Once>
inline static void AddReceiveListener(char* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, Stream& FromStream, uint8_t FromPort) {
	FromStream.setTimeout(-1);
	StreamsInvolved.insert(&FromStream);
	Listeners[FromPort] = [Message, Capacity, Callback = std::move(Callback), &FromStream, FromPort]() {
		if _GLIBCXX14_CONSTEXPR(Once) {
			Async_stream_IO::_InterruptGuard const _;
			Listeners.erase(FromPort);
		}
		uint8_t Length;
		FromStream.readBytes(&Length, sizeof(Length));
		if (Length <= Capacity)
			FromStream.readBytes(Message, Length);
		Callback(Length);
		};
}
template <bool Once>
inline static void AddReceiveListener(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream) {
	FromStream.setTimeout(-1);
	StreamsInvolved.insert(&FromStream);
	Listeners[FromPort] = [Callback = std::move(Callback), &FromStream, FromPort]() {
		if _GLIBCXX14_CONSTEXPR(Once) {
			Async_stream_IO::_InterruptGuard const _;
			Listeners.erase(FromPort);
		}
		interrupts();
		uint8_t Length;
		FromStream.readBytes(&Length, sizeof(Length));
		Callback([&FromStream](void* Message, uint8_t Size) { FromStream.readBytes(reinterpret_cast<char*>(Message), Size); }, Length);
		};
}

// 用于查找一个有效消息的起始
constexpr uint8_t MagicByte = 0x5A;

namespace Async_stream_IO {
	void Send(const void* Message, uint8_t Length, uint8_t ToPort, Stream& BaseStream) {
		BaseStream.setTimeout(-1);
		_InterruptGuard const _;
		BaseStream.write(MagicByte);
		BaseStream.write(ToPort);
		BaseStream.write(Length);
		BaseStream.write(reinterpret_cast<const char*>(Message), Length);
	}
	SendSession::SendSession(uint8_t Length, uint8_t ToPort, Stream& BaseStream) {
		BaseStream.setTimeout(-1);
		BaseStream.write(MagicByte);
		BaseStream.write(ToPort);
		BaseStream.write(Length);
	}

#define DoIfPortIdle(Action)             \
	_InterruptGuard const _;              \
	if (Listeners.contains(FromPort))    \
		return Exception::Port_occupied; \
	Action;                              \
	return Exception::Success;
#define DoWithNewPort(Action)                \
	_InterruptGuard const _;                  \
	const uint8_t FromPort = AllocatePort(); \
	Action;                                  \
	return FromPort;

	Exception Receive(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream) {
		DoIfPortIdle(AddReceiveListener<true>(reinterpret_cast<char*>(Message), Capacity, std::move(Callback), FromStream, FromPort));
	}
	Exception Receive(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream) {
		DoIfPortIdle(AddReceiveListener<true>(std::move(Callback), FromPort, FromStream));
	}
	uint8_t Receive(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, Stream& FromStream) {
		DoWithNewPort(AddReceiveListener<true>(reinterpret_cast<char*>(Message), Capacity, std::move(Callback), FromStream, FromPort));
	}
	uint8_t Receive(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, Stream& FromStream) {
		DoWithNewPort(AddReceiveListener<true>(std::move(Callback), FromPort, FromStream));
	}

	Exception Listen(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream) {
		DoIfPortIdle(AddReceiveListener<false>(reinterpret_cast<char*>(Message), Capacity, std::move(Callback), FromStream, FromPort));
	}
	Exception Listen(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream) {
		DoIfPortIdle(AddReceiveListener<false>(std::move(Callback), FromPort, FromStream));
	}
	uint8_t Listen(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, Stream& FromStream) {
		DoWithNewPort(AddReceiveListener<false>(reinterpret_cast<char*>(Message), Capacity, std::move(Callback), FromStream, FromPort));
	}
	uint8_t Listen(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, Stream& FromStream) {
		DoWithNewPort(AddReceiveListener<false>(std::move(Callback), FromPort, FromStream));
	}

	Exception ReleasePort(uint8_t Port) {
		_InterruptGuard const _;
		return Listeners.erase(Port) ? Exception::Success : Exception::Port_idle;
	}

	void ExecuteTransactionsInQueue() {
		std::set<Stream*> StreamsSwap;
		noInterrupts();
		std::swap(StreamsInvolved, StreamsSwap); // 交换到局部，避免在遍历时被修改
		interrupts();
		for (Stream* STL : StreamsSwap)
			while (STL->available())
				if (STL->read() == MagicByte) {
					uint8_t ToPort;
					STL->readBytes(&ToPort, sizeof(ToPort)); // 保证读入
					noInterrupts();
					std::unordered_map<uint8_t, std::move_only_function<void() const>>::iterator PortListener = Listeners.find(ToPort);
					if (PortListener == Listeners.end()) {
						// 消息指向未监听的端口，丢弃
						interrupts();
						uint8_t Length;
						STL->readBytes(&Length, sizeof(Length));
						STL->readBytes(std::make_unique_for_overwrite<char[]>(Length).get(), Length);
					}
					else {
						std::move_only_function<void() const> Transaction = std::move(PortListener->second);
						interrupts();
						Transaction();
						noInterrupts();
						if ((PortListener = Listeners.find(ToPort)) != Listeners.end())
							PortListener->second = std::move(Transaction);
						interrupts();
					}
				}
		noInterrupts();
		for (Stream* STL : StreamsInvolved) // StreamsInvolved中大概率不会有新的Stream加入，因此这个循环很少真正运行
			StreamsSwap.insert(STL);
		std::swap(StreamsInvolved, StreamsSwap);
		interrupts();
	}
}