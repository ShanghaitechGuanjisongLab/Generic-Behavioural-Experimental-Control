#include "Async_stream_IO.hpp"
#include <queue>
#include <unordered_map>
#include <set>

namespace Async_stream_IO {

// 调用这些方法前必须禁用中断

uint8_t AsyncStream::AllocatePort()const {
	static uint8_t Port = 0;
	while (_Listeners.contains(Port))
		++Port;
	return Port;
}
template<bool Once>
inline static void AddReceiveListener(char* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, AsyncStream* BaseStream, uint8_t FromPort) {
	BaseStream->BaseStream.setTimeout(-1);
	BaseStream->_Listeners[FromPort] = [Message, Capacity, Callback = std::move(Callback), BaseStream, FromPort]() {
		if _GLIBCXX14_CONSTEXPR (Once) {
			_InterruptGuard const _;
			BaseStream->_Listeners.erase(FromPort);
		}
		uint8_t Length;
		BaseStream->BaseStream.readBytes(&Length, sizeof(Length));
		if (Length <= Capacity)
			BaseStream->BaseStream.readBytes(Message, Length);
		Callback(Length);
	};
}
template<bool Once>
inline static void AddReceiveListener(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, uint8_t FromPort, AsyncStream* BaseStream) {
	BaseStream->BaseStream.setTimeout(-1);
	BaseStream->_Listeners[FromPort] = [Callback = std::move(Callback), BaseStream, FromPort]() {
		if _GLIBCXX14_CONSTEXPR (Once) {
			_InterruptGuard const _;
			BaseStream->_Listeners.erase(FromPort);
		}
		interrupts();
		uint8_t Length;
		BaseStream->BaseStream.readBytes(&Length, sizeof(Length));
		Callback([&BaseStream = BaseStream->BaseStream](void* Message, uint8_t Size) {
			BaseStream.readBytes(reinterpret_cast<char*>(Message), Size);
		},
		         Length);
	};
}

// 用于查找一个有效消息的起始
constexpr uint8_t MagicByte = 0x5A;

void AsyncStream::Send(const void* Message, uint8_t Length, uint8_t ToPort) const {
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

#define DoIfPortIdle(Action) \
	_InterruptGuard const _; \
	if (_Listeners.contains(FromPort)) \
		return Exception::Port_occupied; \
	Action; \
	return Exception::Success;
#define DoWithNewPort(Action) \
	_InterruptGuard const _; \
	const uint8_t FromPort = AllocatePort(); \
	Action; \
	return FromPort;

Exception AsyncStream::Receive(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, uint8_t FromPort) {
	DoIfPortIdle(AddReceiveListener<true>(reinterpret_cast<char*>(Message), Capacity, std::move(Callback), this, FromPort));
}
Exception AsyncStream::Receive(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, uint8_t FromPort) {
	DoIfPortIdle(AddReceiveListener<true>(std::move(Callback), FromPort, this));
}
uint8_t AsyncStream::Receive(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback) {
	DoWithNewPort(AddReceiveListener<true>(reinterpret_cast<char*>(Message), Capacity, std::move(Callback), this, FromPort));
}
uint8_t AsyncStream::Receive(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback) {
	DoWithNewPort(AddReceiveListener<true>(std::move(Callback), FromPort, this));
}

Exception AsyncStream::Listen(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, uint8_t FromPort) {
	DoIfPortIdle(AddReceiveListener<false>(reinterpret_cast<char*>(Message), Capacity, std::move(Callback), this, FromPort));
}
Exception AsyncStream::Listen(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, uint8_t FromPort) {
	DoIfPortIdle(AddReceiveListener<false>(std::move(Callback), FromPort, this));
}
uint8_t AsyncStream::Listen(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback) {
	DoWithNewPort(AddReceiveListener<false>(reinterpret_cast<char*>(Message), Capacity, std::move(Callback), this, FromPort));
}
uint8_t AsyncStream::Listen(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback) {
	DoWithNewPort(AddReceiveListener<false>(std::move(Callback), FromPort, this));
}

Exception AsyncStream::ReleasePort(uint8_t Port) {
	_InterruptGuard const _;
	return _Listeners.erase(Port) ? Exception::Success : Exception::Port_idle;
}

void AsyncStream::ExecuteTransactionsInQueue() {
	while (BaseStream.available())
		if (BaseStream.read() == MagicByte) {
			uint8_t ToPort;
			BaseStream.readBytes(&ToPort, sizeof(ToPort));  // 保证读入
			noInterrupts();
			std::unordered_map<uint8_t, std::move_only_function<void() const>>::iterator PortListener = _Listeners.find(ToPort);
			if (PortListener == _Listeners.end()) {
				// 消息指向未监听的端口，丢弃
				interrupts();
				uint8_t Length;
				BaseStream.readBytes(&Length, sizeof(Length));
				BaseStream.readBytes(std::make_unique_for_overwrite<char[]>(Length).get(), Length);
			} else {
				std::move_only_function<void() const> Transaction = std::move(PortListener->second);
				interrupts();
				Transaction();
				noInterrupts();
				if ((PortListener = _Listeners.find(ToPort)) != _Listeners.end())
					PortListener->second = std::move(Transaction);
				interrupts();
			}
		}
}
}