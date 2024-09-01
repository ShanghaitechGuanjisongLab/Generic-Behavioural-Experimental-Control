#include "UID.h"
#include "Async_stream_IO.hpp"
using namespace Async_stream_IO;
void setup()
{
	Serial.setTimeout(-1);
	Serial.begin(9600);
	UID WhichModule;
}
void loop()
{
	ExecuteTransactionsInQueue();
}