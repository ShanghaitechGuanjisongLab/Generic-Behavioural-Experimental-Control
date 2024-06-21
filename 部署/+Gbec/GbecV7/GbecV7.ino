#include "ExperimentDesign.h"
#include "SerialIO.h"
void setup()
{
	Serial.setTimeout(-1);
	Serial.begin(9600);
	SerialWrite(UID::Signal_SerialReady);
}
std::queue<std::function<void()>> SerialReadBuffer;
std::queue<char> SerialWriteBuffer;
std::set<Routine *> Processes;
void loop()
{
	SerialRead<UID>([](UID &&CommandID)
					{
		switch(CommandID)
		{
	case UID::Command_StartProcess:
		SerialRead<UID>([](UID&&RoutineID)
		{
		Routine *const NewProcess = ExportedRoutines::RoutineMap[RoutineID]();
		Processes.insert(NewProcess);
		SerialWrite(UID::Signal_CommandSuccess);
		});
		break;
		} });
	String WriteBuffer;
	noInterrupts();
	while (SerialWriteBuffer.size())
	{
		WriteBuffer += SerialWriteBuffer.front();
		SerialWriteBuffer.pop();
	}
	interrupts();
	Serial.print(WriteBuffer);
}