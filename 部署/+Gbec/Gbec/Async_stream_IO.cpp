#include "Async_stream_IO.hpp"
#include <queue>
#include <unordered_map>
#include <set>

namespace Async_stream_IO {

	// 调用这些方法前必须禁用中断

	uint8_t AsyncStream::_AllocatePort()const {
		static uint8_t Port = 0;
		while (_Listeners.contains(Port))
			++Port;
		return Port;
	}
	uint8_t AsyncStream::AllocatePort() {
		_InterruptGuard const _;
		uint8_t const Port = _AllocatePort();
		_Listeners[Port] = [this](uint8_t MessageSize) {BaseStream.readBytes(std::make_unique_for_overwrite<char[]>(MessageSize).get(), MessageSize);}; // 读入消息，不做任何事，直接丢弃
		return Port;
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

	void AsyncStream::ExecuteTransactionsInQueue() {
		interrupts();
		while (BaseStream.available())
			if (BaseStream.read() == MagicByte) {
#pragma pack(push, 1)
				struct {
					uint8_t ToPort;
					uint8_t Length;
				}Header;
#pragma pack(pop)
				BaseStream.readBytes(reinterpret_cast<char*>(&Header), sizeof(Header));  // 保证读入
				noInterrupts();
				auto PortListener = _Listeners.find(Header.ToPort);
				if (PortListener == _Listeners.end()) {
					// 消息指向未监听的端口，丢弃
					interrupts();
					BaseStream.readBytes(std::make_unique_for_overwrite<char[]>(Header.Length).get(), Header.Length);
				}
				else {
					//先将回调函数取回本地，这样回调函数可以安全释放端口
					std::move_only_function<void(uint8_t MessageSize) const> L = std::move(PortListener->second);
					interrupts();
					L(Header.Length);
					noInterrupts();

					//回调函数中可能会释放该端口，必须先检查
					if ((PortListener = _Listeners.find(Header.ToPort)) != _Listeners.end())
						PortListener->second = std::move(L);
					interrupts();
				}
			}
	}
}