#include "UID.h"
#include "Async_stream_message_queue.hpp"
void setup()
{
	Serial.setTimeout(-1);
	Serial.begin(9600);
	Async_stream_message_queue::Listen([](std::dynarray<char> &&Message)
	{
		
	}, Serial, 0);
}
void loop()
{
	Async_stream_message_queue::ExecuteTransactionsInQueue();
}