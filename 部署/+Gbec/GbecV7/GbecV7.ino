#include "UID.h"
#include "Async_stream_IO.hpp"
using namespace Async_stream_IO;
void setup()
{
	Serial.setTimeout(-1);
	Serial.begin(9600);
	UID WhichModule;
	Listen(&WhichModule,sizeof(WhichModule),[&WhichModule](Exception Result) {
		if(Result==Exception::Success)
		{
			
		}
	},
							static_cast<uint8_t>(UID::Port_CreateProcess));
}
void loop()
{
	ExecuteTransactionsInQueue();
}