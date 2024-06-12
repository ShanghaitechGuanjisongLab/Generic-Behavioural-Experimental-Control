#include "UID.h"
void setup()
{
	Serial.setTimeout(-1);
	Serial.begin(9600);
	SerialWrite(UID::Signal_SerialReady);
}
void loop()
{
	switch (SerialRead<UID>())
	{
	case UID::Command_StartProcess:
	
	}
}