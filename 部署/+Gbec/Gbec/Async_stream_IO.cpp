#include "Async_stream_IO.hpp"
#include <queue>
#include <unordered_map>
#include <set>

namespace Async_stream_IO {

	// 调用这些方法前必须禁用中断

	uint8_t AsyncStream::_AllocatePort() const {
		static uint8_t Port = 0;
		while (_Listeners.contains(Port))
			Port = (Port + 1) % 255;
		return Port;
	}
	uint8_t AsyncStream::AllocatePort() {
		InterruptGuard const _;
		uint8_t const Port = _AllocatePort();
		_Listeners[Port] = [this](uint8_t MessageSize) {
			Skip(MessageSize);
			};
		return Port;
	}

	// 用于查找一个有效消息的起始
	constexpr uint8_t MagicByte = 0x5A;

	void AsyncStream::Send(const void* Message, uint8_t Length, uint8_t ToPort) const {
		InterruptGuard const _;
		BaseStream.write(MagicByte);
		BaseStream.write(ToPort);
		BaseStream.write(Length);
		BaseStream.write(reinterpret_cast<const char*>(Message), Length);
	}
	SendSession::SendSession(uint8_t Length, uint8_t ToPort, Stream& BaseStream)
		: BaseStream(BaseStream) {
		BaseStream.setTimeout(-1);
		BaseStream.write(MagicByte);
		BaseStream.write(ToPort);
		BaseStream.write(Length);
	}
	void AsyncStream::PortForward(AsioHeader Header) {
		noInterrupts();
		auto PortListener = _Listeners.find(Header.ToPort);
		if (PortListener == _Listeners.end()) {
			// 消息指向未监听的端口，丢弃
			interrupts();
			Skip(Header.Length);
			return;
		}
		//先将回调函数取回本地，这样回调函数可以安全释放端口
		std::move_only_function<void(uint8_t) const> L = std::move(PortListener->second);
		PortListener->second = nullptr;
		interrupts();
		L(Header.Length);
		noInterrupts();

		//检查回调函数是否释放或重新分配了端口。如果释放了，不能满足条件1；如果重新分配了，不能满足条件2。只有未发生这两种情况，才能满足条件1和2，才能将取回本地的回调方法重新放回容器。
		if ((PortListener = _Listeners.find(Header.ToPort)) != _Listeners.end() && PortListener->second == nullptr)
			PortListener->second = std::move(L);
		interrupts();
	}
	void AsyncStream::ExecuteTransactionsInQueue() {
		interrupts();
		while (BaseStream.available())
			if (BaseStream.read() == MagicByte)
				PortForward(Read<AsioHeader>());
	}
	uint8_t AsyncStream::Listen(uint8_t FromPort) {
		for (;;) {
			if (Read<uint8_t>() == MagicByte) {
				AsioHeader const Header = Read<AsioHeader>();
				if (Header.ToPort == FromPort)
					return Header.Length;
				PortForward(Header);
			}
		}
	}
	void AsyncStream::Skip(uint8_t Length) const {
		while (Length)
			if (BaseStream.read() != -1)
				--Length;
	}
}