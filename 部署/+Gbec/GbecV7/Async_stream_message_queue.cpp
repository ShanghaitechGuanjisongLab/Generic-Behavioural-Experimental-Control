#include "Async_stream_message_queue.hpp"
#include <queue>
#include <unordered_map>
#include <set>
static std::queue<std::move_only_function<void() const>> TransactionQueue;
static std::unordered_map<uint8_t, std::move_only_function<void() const>> Listeners;
static std::set<Stream &> StreamsInvolved;
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
	FromStream.setTimeout(-1);
	StreamsInvolved.insert(FromStream);
	Listeners[FromPort] = [Message, Capacity, Callback = std::move(Callback), &FromStream, FromPort]()
	{
		if _CONSTEXPR14 ()
			(Once)
				Listeners.erase(FromPort);
		interrupts();
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
}
template <bool Once>
void AddReceiveListener(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream, uint8_t FromPort)
{
	FromStream.setTimeout(-1);
	StreamsInvolved.insert(FromStream);
	Listeners[FromPort] = [Callback = std::move(Callback), &FromStream, FromPort]()
	{
		if _CONSTEXPR14 ()
			(Once)
				Listeners.erase(FromPort);
		interrupts();
		uint8_t Length;
		FromStream.readBytes(&Length, sizeof(Length));
		std::dynarray<char> Message(Length);
		FromStream.readBytes(Message.data(), Length);
		Callback(std::move(Message));
	};
}
#define InterruptiveReturn(Result) \
	{                              \
		if (HasInterrupts)         \
			interrupts();          \
		return Result;             \
	}
#define InterruptiveCheckPort                              \
	const bool HasInterrupts = SaveAndDisableInterrupts(); \
	if (Listeners.contains(FromPort))                      \
		InterruptiveReturn(Exception::Port_occupied);
// 用于查找一个有效消息的起始
constexpr uint8_t MagicByte = 0x5A;
namespace Async_stream_message_queue
{
	void Send(const char *Message, uint8_t Length, Stream &ToStream, uint8_t ToPort, std::move_only_function<void() const> &&Callback)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		StreamsInvolved.insert(ToStream);
		ToStream.setTimeout(-1);
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
		StreamsInvolved.insert(ToStream);
		ToStream.setTimeout(-1);
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

	Exception ReleasePort(uint8_t Port)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		const bool AnyErased = Listeners.erase(Port);
		InterruptiveReturn(AnyErased ? Exception::Success : Exception::Port_idle);
	}

	void ExecuteTransactionsInQueue()
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		while (TransactionQueue.size())
		{
			const std::move_only_function<void() const> Transaction = std::move(TransactionQueue.front());
			TransactionQueue.pop();
			interrupts();
			Transaction();
			noInterrupts();
		}
		std::set<Stream &> StreamsSwap;
		std::swap(StreamsInvolved, StreamsSwap); // 交换到局部，避免在遍历时被修改
		interrupts();
		for (Stream &STL : StreamsSwap)
		{
			while (STL.available())
				if (STL.read() == MagicByte)
				{
					uint8_t ToPort;
					STL.readBytes(&ToPort, sizeof(ToPort)); // 保证读入
					noInterrupts();
					std::unordered_map<uint8_t, std::move_only_function<void() const>>::iterator PortListener = Listeners.find(ToPort);
					if (PortListener == Listeners.end())
					{
						interrupts();
						uint8_t Length;
						STL.readBytes(&Length, sizeof(Length));
						while (Length)
							if (STL.read() != -1)
								Length--;
					}
					else
					{
						std::move_only_function<void() const> Transaction = std::move(PortListener->second);
						Transaction();
						noInterrupts();
						if ((PortListener = Listeners.find(ToPort)) != Listeners.end())
							PortListener->second = std::move(Transaction);
						interrupts();
					}
				}
		}
		noInterrupts();
		for (Stream &STL : StreamsInvolved) // StreamsInvolved中大概率不会有新的Stream加入，因此这个循环很少真正运行
			StreamsSwap.insert(STL);
		std::swap(StreamsInvolved, StreamsSwap);
		InterruptiveReturn();
	}
}