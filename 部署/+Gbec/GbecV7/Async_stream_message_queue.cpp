#include "Async_stream_message_queue.hpp"
#include <queue>
#include <unordered_map>
#include <set>
static std::queue<std::move_only_function<void() const>> TransactionQueue;
static std::unordered_map<uint8_t, std::move_only_function<void() const>> Listeners;
// Listeners的交换必须放在全局，这样执行期间的Receive/Listen中断调用才能检查到Port是否被占用。
static std::unordered_map<uint8_t, std::move_only_function<void() const>> Listeners_Swap;
static std::set<uint8_t> ListenersToRelease;
static std::set<Stream &> StreamsToListen;
bool SaveAndDisableInterrupts()
{
	const bool Enabled =
#ifdef ARDUINO_ARCH_AVR
		bitRead(SREG, SREG_I)
#endif
#ifdef ARDUINO_ARCH_SAM
			!__get_FAULTMASK()
#endif
		;
	noInterrupts();
	return Enabled;
}
uint8_t AllocatePort()
{
	uint8_t Port = 0;
	while (Listeners.contains(Port))
		++Port;
	return Port;
}
using namespace Async_stream_message_queue;
template <bool Once>
void AddReceiveListener(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream, uint8_t FromPort)
{
	Listeners[FromPort] = [Message, Capacity, Callback = std::move(Callback), &FromStream, FromPort]()
	{
		if (Once)
			ListenersToRelease.insert(FromPort);
		uint8_t Length;
		FromStream.readBytes(&Length, sizeof(Length));
		if (Length > Capacity)
			Callback(Exception::Insufficient_message_capacity);
		else
		{
			FromStream.readBytes(Message, Length);
			Callback(Exception::Success);
		}
	};
	StreamsToListen.insert(FromStream);
}
template <bool Once>
void AddReceiveListener(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream, uint8_t FromPort)
{
	Listeners[FromPort] = [Callback = std::move(Callback), &FromStream, FromPort]()
	{
		if (Once)
			ListenersToRelease.insert(FromPort);
		uint8_t Length;
		FromStream.readBytes(&Length, sizeof(Length));
		std::dynarray<char> Message(Length);
		FromStream.readBytes(Message.data(), Length);
		Callback(std::move(Message));
	};
	StreamsToListen.insert(FromStream);
}
#define InterruptiveReturn(Result) \
	{                              \
		if (HasInterrupts)         \
			interrupts();          \
		return Result;             \
	}
#define InterruptiveCheckPort                                              \
	const bool HasInterrupts = SaveAndDisableInterrupts();                 \
	if (Listeners.contains(FromPort) || Listeners_Swap.contains(FromPort)) \
		InterruptiveReturn(Exception::Port_occupied);
// 用于查找一个有效消息的起始
constexpr uint8_t MagicByte = 0x5A;
namespace Async_stream_message_queue
{
	void Send(const char *Message, uint8_t Length, Stream &ToStream, uint8_t ToPort, std::move_only_function<void() const> &&Callback)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		TransactionQueue.push([Message, Length, &ToStream, ToPort, Callback = std::move(Callback)]()
							  { 
								ToStream.write(MagicByte);
						ToStream.write(ToPort); 
						ToStream.write(Length);
						ToStream.write(Message,Length); 
						Callback(); });
		InterruptiveReturn();
	}
	void Send(std::dynarray<char> &&Message, Stream &ToStream, uint8_t ToPort, std::move_only_function<void() const> &&Callback)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		TransactionQueue.push([Message = std::move(Message), &ToStream, ToPort, Callback = std::move(Callback)]()
							  { 
								ToStream.write(MagicByte);
						ToStream.write(ToPort); 
						ToStream.write((uint8_t)Message.size());
						ToStream.write(Message.data(),Message.size()); 
						Callback(); });
		InterruptiveReturn();
	}

	Exception Receive(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream, uint8_t FromPort)
	{
		InterruptiveCheckPort;
		AddReceiveListener<true>(Message, Capacity, std::move(Callback), FromStream, FromPort);
		InterruptiveReturn(Exception::Success);
	}
	Exception Receive(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream, uint8_t FromPort)
	{
		InterruptiveCheckPort;
		AddReceiveListener<true>(std::move(Callback), FromStream, FromPort);
		InterruptiveReturn(Exception::Success);
	}
	uint8_t Receive(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		const uint8_t FromPort = AllocatePort();
		AddReceiveListener<true>(Message, Capacity, std::move(Callback), FromStream, FromPort);
		InterruptiveReturn(FromPort);
	}
	uint8_t Receive(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		const uint8_t FromPort = AllocatePort();
		AddReceiveListener<true>(std::move(Callback), FromStream, FromPort);
		InterruptiveReturn(FromPort);
	}
	Exception Listen(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream, uint8_t FromPort)
	{
		InterruptiveCheckPort;
		AddReceiveListener<false>(Message, Capacity, std::move(Callback), FromStream, FromPort);
		InterruptiveReturn(Exception::Success);
	}
	Exception Listen(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream, uint8_t FromPort)
	{
		InterruptiveCheckPort;
		AddReceiveListener<false>(std::move(Callback), FromStream, FromPort);
		InterruptiveReturn(Exception::Success);
	}
	uint8_t Listen(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		const uint8_t FromPort = AllocatePort();
		AddReceiveListener<false>(Message, Capacity, std::move(Callback), FromStream, FromPort);
		InterruptiveReturn(FromPort);
	}
	uint8_t Listen(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		const uint8_t FromPort = AllocatePort();
		AddReceiveListener<false>(std::move(Callback), FromStream, FromPort);
		InterruptiveReturn(FromPort);
	}

	void ReleasePort(uint8_t Port, std::move_only_function<void(Exception Result) const> &&Callback)
	{
		//必须延迟释放端口，因为这个函数可能是被监听器Callback调用的，此时如果释放了监听器自身，将导致未定义行为
		const bool HasInterrupts = SaveAndDisableInterrupts();
		TransactionQueue.push([Port, Callback = std::move(Callback)]()
							  { 
								noInterrupts();
								const bool AnyErased = Listeners.erase(Port);
								interrupts();
								Callback(AnyErased ? Exception::Success : Exception::Port_idle); });
		InterruptiveReturn();
	}

	void ExecuteTransactionsInQueue()
	{
		noInterrupts();
		// 在无中断模式下将全局容器交换到局部，避免争用。事后再换回去。
		std::set<Stream &> StreamsToListen_Swap;
		std::swap(StreamsToListen_Swap, StreamsToListen);
		std::queue<std::move_only_function<void() const>> TransactionQueue_Swap;
		std::swap(TransactionQueue_Swap, TransactionQueue);
		std::swap(Listeners_Swap, Listeners);
		interrupts();
		for (Stream &STL : StreamsToListen_Swap)
			STL.setTimeout(-1); // 无限等待
		while (TransactionQueue_Swap.size())
		{
			TransactionQueue_Swap.front()();
			TransactionQueue_Swap.pop();
		}
		for (Stream &STL : StreamsToListen_Swap)
			while (STL.available())
				if (STL.read() == MagicByte)
				{
					uint8_t ToPort;
					STL.readBytes(&ToPort, sizeof(ToPort)); // 保证读入
					const std::unordered_map<uint8_t, std::move_only_function<void() const>>::const_iterator PortListener = Listeners_Swap.find(ToPort);
					if (PortListener == Listeners_Swap.end())
					{
						uint8_t Length;
						STL.readBytes(&Length, sizeof(Length));
						while (Length)
							if (STL.read() != -1)
								Length--;
					}
					else
					{
						PortListener->second();
						break;
					}
				}
	}
}