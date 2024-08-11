#pragma once
#include <Cpp_Standard_Library.h>
#include <queue>
#include <functional>
#include <dynarray>
// 每个具体的消息应当继承自此消息
struct MessageHeader
{
	uint8_t From;
	uint8_t To;
};
extern std::queue<char> SerialWriteBuffer;
extern std::queue<std::function<void()>> SerialReadBuffer;
template <typename T>
void SerialWrite(T Value)
{
	union
	{
		T Any;
		char Array[sizeof(T)];
	} AnyToArray{.Any = Value};
	noInterrupts();
	for (const char C : AnyToArray.Array)
		SerialWriteBuffer.push(C);
	interrupts();
}
template <typename T>
void SerialRead(std::function<void(T &&)> &&Callback)
{
	noInterrupts();
	SerialReadBuffer.push([Callback]()
						  {
		T Value;
		Serial.readBytes((char*)Value,sizeof(T));
		Callback(std::move(Value)); });
	interrupts();
}