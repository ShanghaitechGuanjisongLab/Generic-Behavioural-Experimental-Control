#include "UID.h"
#include "Objects.h"
#include <functional>
#include <map>
#include <queue>
#include <set>

// 一个魔数，表示接下来要开始的是有效的报文信息
constexpr uint8_t SerialReady = 17;

/*消息头结构
{
	uint8_t SerialReady;必须是SerialReady常数，标志消息头
	uint8_t To;目标对象ID
	uint8_t Length;消息长度，不包括消息头
}
*/

// 全局IO缓冲，不允许中断

std::map<uint8_t, std::move_only_function<void(uint8_t Size, Stream &Message) const>> ListeningPorts;
std::queue<MessageHeader> MessageQueue;
std::set<std::function<void()>> IdleTasks;

LocalObject::LocalObject()
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
			struct
			{
				uint8_t To;
				// 这个长度不包括消息头
				uint8_t Length;
			} Header;
			Serial.readBytes((char *)&Header, sizeof(Header));
			std::dynarray<char> Message(Header.Length);
			Serial.readBytes(Message.get(), Message.size());
			noInterrupts();
			ListeningPorts[Header.To](Header.Length, Serial);
			interrupts();
		}
	}
	noInterrupts();
	while (MessageQueue.size())
	{
		Serial.write(SerialReady);
		const MessageHeader&Message=MessageQueue.front(); 
		Serial.write((uint8_t)MessageQueue.front().size());
		Serial.write(MessageQueue.front().get(), MessageQueue.front().size());
		MessageQueue.pop();
	}
	for (const std::function<void()> &Task : IdleTasks)
		Task();
	interrupts();
}