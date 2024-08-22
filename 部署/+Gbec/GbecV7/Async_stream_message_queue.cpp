#include "Async_stream_message_queue.hpp"
#include <queue>
#include <unordered_map>
#include <set>
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
namespace Async_stream_message_queue
{
	static std::queue<std::move_only_function<void() const>> TransactionQueue;
	void Send(const char *Message, uint8_t Length, Stream &ToStream, uint8_t ToPort, std::move_only_function<void() const> &&Callback)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		TransactionQueue.push([Message, Length, &ToStream, ToPort, Callback = std::move(Callback)]()
							  { 
						ToStream.write(ToPort); 
						ToStream.write(Length);
						ToStream.write(Message,Length); 
						Callback(); });
		if (HasInterrupts)
			interrupts();
	}
	void Send(std::dynarray<char> &&Message, Stream &ToStream, uint8_t ToPort, std::move_only_function<void() const> &&Callback)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		TransactionQueue.push([Message = std::move(Message), &ToStream, ToPort, Callback = std::move(Callback)]()
							  { 
						ToStream.write(ToPort); 
						ToStream.write((uint8_t)Message.size());
						ToStream.write(Message.data(),Message.size()); 
						Callback(); });
		if (HasInterrupts)
			interrupts();
	}

	static std::unordered_map<uint8_t, std::move_only_function<void() const>> Listeners;
	static std::set<uint8_t> ListenersToRelease;
	Exception Receive(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream, uint8_t FromPort)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		if (Listeners.contains(FromPort))
		{
			if (HasInterrupts)
				interrupts();
			return Exception::Port_occupied;
		}
		Listeners[FromPort] = [Message, Capacity, Callback = std::move(Callback), &FromStream, FromPort]()
		{
			ListenersToRelease.insert(FromPort);
			const uint8_t Length = FromStream.read();
			if (Length > Capacity)
				Callback(Exception::Insufficient_message_capacity);
			else
			{
				FromStream.readBytes(Message, Length);
				Callback(Exception::Success);
			}
		};
		if (HasInterrupts)
			interrupts();
		return Exception::Success;
	}
	Exception Receive(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream, uint8_t FromPort)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		if (Listeners.contains(FromPort))
		{
			if (HasInterrupts)
				interrupts();
			return Exception::Port_occupied;
		}
		Listeners[FromPort] = [Callback = std::move(Callback), &FromStream, FromPort]()
		{
			ListenersToRelease.insert(FromPort);
			const uint8_t Length = FromStream.read();
			std::dynarray<char> Message(Length);
			FromStream.readBytes(Message.data(), Length);
			Callback(std::move(Message));
		};
		if (HasInterrupts)
			interrupts();
		return Exception::Success;
	}
	uint8_t Receive(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		uint8_t FromPort = 0;
		while (Listeners.contains(FromPort))
			++FromPort;
		Listeners[FromPort] = [Message, Capacity, Callback = std::move(Callback), &FromStream, FromPort]()
		{
			ListenersToRelease.insert(FromPort);
			const uint8_t Length = FromStream.read();
			if (Length > Capacity)
				Callback(Exception::Insufficient_message_capacity);
			else
			{
				FromStream.readBytes(Message, Length);
				Callback(Exception::Success);
			}
		};
		if (HasInterrupts)
			interrupts();
		return FromPort;
	}
	uint8_t Receive(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		uint8_t FromPort = 0;
		while (Listeners.contains(FromPort))
			++FromPort;
		Listeners[FromPort] = [Callback = std::move(Callback), &FromStream, FromPort]()
		{
			ListenersToRelease.insert(FromPort);
			const uint8_t Length = FromStream.read();
			std::dynarray<char> Message(Length);
			FromStream.readBytes(Message.data(), Length);
			Callback(std::move(Message));
		};
		if (HasInterrupts)
			interrupts();
		return FromPort;
	}
	Exception Listen(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream, uint8_t FromPort)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		if (Listeners.contains(FromPort))
		{
			if (HasInterrupts)
				interrupts();
			return Exception::Port_occupied;
		}
		Listeners[FromPort] = [Message, Capacity, Callback = std::move(Callback), &FromStream, FromPort]()
		{
			const uint8_t Length = FromStream.read();
			if (Length > Capacity)
				Callback(Exception::Insufficient_message_capacity);
			else
			{
				FromStream.readBytes(Message, Length);
				Callback(Exception::Success);
			}
		};
		if (HasInterrupts)
			interrupts();
		return Exception::Success;
	}
	Exception Listen(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream, uint8_t FromPort)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		if (Listeners.contains(FromPort))
		{
			if (HasInterrupts)
				interrupts();
			return Exception::Port_occupied;
		}
		Listeners[FromPort] = [Callback = std::move(Callback), &FromStream, FromPort]()
		{
			const uint8_t Length = FromStream.read();
			std::dynarray<char> Message(Length);
			FromStream.readBytes(Message.data(), Length);
			Callback(std::move(Message));
		};
		if (HasInterrupts)
			interrupts();
		return Exception::Success;
	}
	uint8_t Listen(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		uint8_t FromPort = 0;
		while (Listeners.contains(FromPort))
			++FromPort;
		Listeners[FromPort] = [Message, Capacity, Callback = std::move(Callback), &FromStream, FromPort]()
		{
			const uint8_t Length = FromStream.read();
			if (Length > Capacity)
				Callback(Exception::Insufficient_message_capacity);
			else
			{
				FromStream.readBytes(Message, Length);
				Callback(Exception::Success);
			}
		};
		if (HasInterrupts)
			interrupts();
		return FromPort;
	}
	uint8_t Listen(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		uint8_t FromPort = 0;
		while (Listeners.contains(FromPort))
			++FromPort;
		Listeners[FromPort] = [Callback = std::move(Callback), &FromStream, FromPort]()
		{
			const uint8_t Length = FromStream.read();
			std::dynarray<char> Message(Length);
			FromStream.readBytes(Message.data(), Length);
			Callback(std::move(Message));
		};
		if (HasInterrupts)
			interrupts();
		return FromPort;
	}

	void ReleasePort(uint8_t Port, std::move_only_function<void(Exception Result) const> &&Callback)
	{
		const bool HasInterrupts = SaveAndDisableInterrupts();
		TransactionQueue.push([Port, Callback = std::move(Callback)]()
							  { 
								Callback(Listeners.erase(Port) ? Exception::Success : Exception::Port_idle); 
								});
		if (HasInterrupts)
			interrupts();
	}

	void ExecuteTransactionsInQueue()
	{
		//串口发送事务必须允许中断
		const size_t NumberToExecute = TransactionQueue.size(); // 仅执行当前队列中的事务，不执行后续加入的事务
		for (size_t T = 0; T < NumberToExecute; ++T)
		{
			TransactionQueue.front()();
			TransactionQueue.pop();
		}
	}
}