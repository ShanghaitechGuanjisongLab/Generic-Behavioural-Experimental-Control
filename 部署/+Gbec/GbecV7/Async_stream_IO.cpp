#include "Async_stream_IO.hpp"
#include <queue>
#include <unordered_map>
#include <set>
static std::queue<std::move_only_function<void() const>> TransactionQueue;
static std::unordered_map<uint8_t, std::move_only_function<void() const>> Listeners;
static std::set<Stream*> StreamsInvolved;
static bool SaveAndDisableInterrupts()
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
static uint8_t AllocatePort()
{
	uint8_t Port = 0;
	while (Listeners.contains(Port))
		++Port;
	return Port;
}
using namespace Async_stream_IO;
template <bool Once>
inline void AddReceiveListener(char* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, Stream& FromStream, uint8_t FromPort)
{
	FromStream.setTimeout(-1);
	StreamsInvolved.insert(&FromStream);
	Listeners[FromPort] = [Message, Capacity, Callback = std::move(Callback), &FromStream, FromPort]()
		{
			if _CONSTEXPR14()
				(Once)
				Listeners.erase(FromPort);
			interrupts();
			uint8_t Length;
			FromStream.readBytes(&Length, sizeof(Length));
			if (Length <= Capacity)
				FromStream.readBytes(Message, Length);
			Callback(Length);
		};
}
template <bool Once>
inline void AddReceiveListener(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream)
{
	FromStream.setTimeout(-1);
	StreamsInvolved.insert(&FromStream);
	Listeners[FromPort] = [Callback = std::move(Callback), &FromStream, FromPort]()
		{
			if _CONSTEXPR14()
				(Once)
				Listeners.erase(FromPort);
			interrupts();
			uint8_t Length;
			FromStream.readBytes(&Length, sizeof(Length));
			Callback([&FromStream](void* Message, uint8_t Size)
				{ FromStream.readBytes(reinterpret_cast<char*>(Message), Size); }, Length);
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
namespace Async_stream_IO
{
	void Send(const void* Message, uint8_t Length, uint8_t ToPort, Stream& ToStream)
	{
		ToStream.setTimeout(-1);
		const bool HasInterrupts = SaveAndDisableInterrupts();
		StreamsInvolved.insert(&ToStream);
		TransactionQueue.push([Message, Length, &ToStream, ToPort]()
			{
				ToStream.write(MagicByte);
				ToStream.write(ToPort);
				ToStream.write(Length);
				ToStream.write(reinterpret_cast<const char*>(Message), Length); });
		InterruptiveReturn();
	}
	void Send(std::move_only_function<void(const std::move_only_function<void(const void* Message, uint8_t Size) const>& MessageSender) const>&& Callback, uint8_t ToPort, Stream& ToStream = Serial)
	{
		ToStream.setTimeout(-1);
		const bool HasInterrupts = SaveAndDisableInterrupts();
		StreamsInvolved.insert(&ToStream);
		TransactionQueue.push([&ToStream, ToPort, Callback = std::move(Callback)]()
			{ Callback([&ToStream, ToPort](const void* Message, uint8_t Size)
				{
					ToStream.write(MagicByte);
					ToStream.write(ToPort);
					ToStream.write(Size);
					ToStream.write(reinterpret_cast<const char*>(Message), Size); }); });
		InterruptiveReturn();
	}

	Exception Receive(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream)
	{
		InterruptiveCheckPort;
		AddReceiveListener<true>(reinterpret_cast<char*>(Message), Capacity, std::move(Callback), FromStream, FromPort);
		InterruptiveReturn(Exception::Success);
	}
	Exception Receive(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream = Serial)
	{
		InterruptiveCheckPort;
		AddReceiveListener<true>(std::move(Callback), FromPort, FromStream);
		InterruptiveReturn(Exception::Success);
	}
	uint8_t Receive(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, Stream& FromStream)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		const uint8_t FromPort = AllocatePort();
		AddReceiveListener<true>(reinterpret_cast<char*>(Message), Capacity, std::move(Callback), FromStream, FromPort);
		InterruptiveReturn(FromPort);
	}
	uint8_t Receive(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, Stream& FromStream = Serial)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		const uint8_t FromPort = AllocatePort();
		AddReceiveListener<true>(std::move(Callback), FromPort, FromStream);
		InterruptiveReturn(FromPort);
	}

	Exception Listen(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream)
	{
		InterruptiveCheckPort;
		AddReceiveListener<false>(reinterpret_cast<char*>(Message), Capacity, std::move(Callback), FromStream, FromPort);
		InterruptiveReturn(Exception::Success);
	}
	Exception Listen(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream = Serial)
	{
		InterruptiveCheckPort;
		AddReceiveListener<false>(std::move(Callback), FromPort, FromStream);
		InterruptiveReturn(Exception::Success);
	}
	uint8_t Listen(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, Stream& FromStream)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		const uint8_t FromPort = AllocatePort();
		AddReceiveListener<false>(reinterpret_cast<char*>(Message), Capacity, std::move(Callback), FromStream, FromPort);
		InterruptiveReturn(FromPort);
	}
	uint8_t Listen(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, Stream& FromStream = Serial)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		const uint8_t FromPort = AllocatePort();
		AddReceiveListener<false>(std::move(Callback), FromPort, FromStream);
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
		static std::queue<std::move_only_function<void() const>> TransactionQueueSwap;
		noInterrupts();
		std::swap(TransactionQueue, TransactionQueueSwap); // 交换到局部，确保执行期间新产生的事务延迟到下一次执行
		interrupts();
		while (TransactionQueue.size())
		{
			TransactionQueue.front()();
			TransactionQueue.pop();
		}
		std::set<Stream*> StreamsSwap;
		noInterrupts();
		std::swap(StreamsInvolved, StreamsSwap); // 交换到局部，避免在遍历时被修改
		interrupts();
		for (Stream* STL : StreamsSwap)
		{
			while (STL->available())
				if (STL->read() == MagicByte)
				{
					uint8_t ToPort;
					STL->readBytes(&ToPort, sizeof(ToPort)); // 保证读入
					noInterrupts();
					std::unordered_map<uint8_t, std::move_only_function<void() const>>::iterator PortListener = Listeners.find(ToPort);
					if (PortListener == Listeners.end())
					{
						interrupts();
						uint8_t Length;
						STL->readBytes(&Length, sizeof(Length));
						STL->readBytes(std::make_unique_for_overwrite<char[]>(Length).get(), Length);
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
		for (Stream* STL : StreamsInvolved) // StreamsInvolved中大概率不会有新的Stream加入，因此这个循环很少真正运行
			StreamsSwap.insert(STL);
		std::swap(StreamsInvolved, StreamsSwap);
		interrupts();
	}
}