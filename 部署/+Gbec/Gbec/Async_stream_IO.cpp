#include "Async_stream_IO.hpp"
#include <queue>
#include <unordered_map>
#include <set>
extern bool Debug;
namespace Async_stream_IO {

// 调用这些方法前必须禁用中断

Port AsyncStream::_AllocatePort() const {
	static Port P = 0;
	while (_Listeners.contains(P))
		P = (P + 1) % 255;
	return P;
}
Port AsyncStream::AllocatePort() {
	InterruptGuard const _;
	Port const P = _AllocatePort();
	_Listeners[P] = [this](MessageSize MS) {
		Skip(MS);
	};
	return P;
}

// 用于查找一个有效消息的起始
constexpr uint8_t MagicByte = 0x5A;

template<typename T>
inline std::enable_if_t<(sizeof(T) > 1), Stream> &operator<<(Stream &S, T const &Value) {
	S.write(reinterpret_cast<const char *>(&Value), sizeof(T));
	return S;
}
template<typename T>
inline std::enable_if_t<sizeof(T) == 1, Stream> &operator<<(Stream &S, T const &Value) {
	S.write(Value);
	return S;
}

void AsyncStream::Send(const void *Message, MessageSize Length, Port ToPort) const {
	InterruptGuard const _;
	BaseStream << MagicByte << ToPort << Length;
	BaseStream.write(reinterpret_cast<const char *>(Message), Length);
}
SendSession::SendSession(MessageSize Length, Port ToPort, Stream &BaseStream)
  : BaseStream(BaseStream) {
	BaseStream << MagicByte << ToPort << Length;
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
	// 先将回调函数取回本地，这样回调函数可以安全释放端口
	std::move_only_function<void(MessageSize) const> L = std::move(PortListener->second);
	PortListener->second = nullptr;
	interrupts();
	L(Header.Length);
	noInterrupts();

	// 检查回调函数是否释放或重新分配了端口。如果释放了，不能满足条件1；如果重新分配了，不能满足条件2。只有未发生这两种情况，才能满足条件1和2，才能将取回本地的回调方法重新放回容器。
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
MessageSize AsyncStream::Listen(Port FromPort) {
	for (;;) {
		if (Read<decltype(MagicByte)>() == MagicByte) {
			AsioHeader const Header = Read<AsioHeader>();
			if (Header.ToPort == FromPort)
				return Header.Length;
			PortForward(Header);
		}
	}
}
void AsyncStream::Skip(MessageSize Length) const {
	while (Length)
		if (BaseStream.read() != -1)
			--Length;
}
}