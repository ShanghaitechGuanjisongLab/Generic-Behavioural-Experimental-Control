#include <Cpp_Standard_Library.h>
#include <functional>
#include <optional>
#include <vector>
// 一个魔数，表示接下来要开始的是有效的报文信息
constexpr char SerialReady = 17;
void setup()
{
	Serial.setTimeout(-1);
	Serial.begin(9600);
}
// 对此对象的任何访问都不允许中断
std::vector<std::optiostd::move_only_function<void(std::unique_ptr<char[]> &&) const>> ListeningPorts;
void loop()
{
	// 主循环要做的事情就只有转发报文
	while (Serial.available())
	{
		if (Serial.read() == SerialReady)
		{
			struct MessageHeader
			{
				// 这个长度不包括消息头
				uint8_t Length;
				uint8_t To;
			} Header;
			Serial.readBytes((char *)&Header, sizeof(Header));
			std::unique_ptr<char[]> Message = std::make_unique_for_overwrite<char[]>(Header.Length);
			Serial.readBytes(Message.get(), Header.Length);
			noInterrupts();
			ListeningPorts[Header.To](std::move(Message));
			interrupts();
		}
	}

}