#include "UID.h"
#include "Remote.h"
#include <functional>
#include <queue>
#include <set>
#include <unordered_set>

// 一个魔数，表示接下来要开始的是有效的报文信息
constexpr uint8_t SerialReady = 17;

/*消息头结构
{
	uint8_t SerialReady;必须是SerialReady常数，标志消息头
	uint32_t To;目标对象ID
	uint16_t Length;消息长度，不包括消息头
}
*/

struct MessageToSend
{
	uint8_t To;
	std::dynarray<char> Message;
};

// 全局IO缓冲，不允许中断

std::unordered_set<const std::move_only_function<void(uint8_t Size, Stream &Message) const> *> ListeningPorts;
std::queue<MessageToSend> MessageQueue;
std::set<std::function<void()>> IdleTasks;

void SendMessage(uint8_t To, std::dynarray<char> &&Message)
{
	MessageQueue.push({To, std::move(Message)});
}
uint8_t StartListen(std::move_only_function<void(uint8_t Size, Stream &Message) const> &&Listener)
{
}
void setup()
{
	Serial.setTimeout(-1);
	Serial.begin(9600);
	ListeningPorts[0] = [](uint8_t Size, Stream &Message)
	{
		struct
		{
			uint8_t ReturnTo;
			UID Function;
		};
	};
}
void loop()
{
	// 主循环要做的事情就只有转发报文
	while (Serial.available())
	{
		if (Serial.read() == SerialReady)
		{
#pragma pack(push, 2)
			struct
			{
				union 
				{
					const std::move_only_function<void(uint8_t Size, Stream &Message) const> *Listener;
					uint32_t To;
				};				
				// 这个长度不包括消息头
				uint16_t Length;
			} Header;
#pragma pack(pop)
			Serial.readBytes((char *)&Header, sizeof(Header));
			std::dynarray<char> Message(Header.Length);
			Serial.readBytes(Message.get(), Message.size());
			noInterrupts();
			const bool Valid=ListeningPorts.con
			interrupts();
		}
	}
	noInterrupts();
	while (MessageQueue.size())
	{
		Serial.write(SerialReady);
		const MessageToSend &Message = MessageQueue.front();
		Serial.write(Message.To);
		Serial.write((uint8_t)Message.Message.size());
		Serial.write(Message.Message.get(), Message.Message.size());
		MessageQueue.pop();
	}
	for (const std::function<void()> &Task : IdleTasks)
		Task();
	interrupts();
}