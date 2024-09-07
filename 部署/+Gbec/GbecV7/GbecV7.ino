#include "Objects.h"
#include <set>
template <typename T>
inline void BindFunctionToPort(T &&Function, UID Port)
{
	Async_stream_IO::BindFunctionToPort(std::forward<T>(Function), static_cast<uint8_t>(Port));
}
std::set<Object *> AllObjects;
void setup()
{
	Serial.setTimeout(-1);
	Serial.begin(9600);
	BindFunctionToPort([]()
					   { return static_cast<uint8_t>(sizeof(size_t)); }, UID::Port_PointerSize);
	BindFunctionToPort([](UID Subclass)
					   {
		const auto Iterator = ObjectCreators.find(Subclass);
		if (Iterator == ObjectCreators.end())
			return static_cast<Object *>(nullptr);
		Object*const NewObject=Iterator->second();
		AllObjects.insert(NewObject);
		return NewObject; }, UID::Port_ObjectCreate);
	BindFunctionToPort([](Object *O)
					   {
		if(AllObjects.contains(O))
			return O->Start();
		else
			return UID::Exception_InvalidObject; }, UID::Port_ObjectStart);
	BindFunctionToPort([](Object *O,uint16_t Times)
					   {
		if(AllObjects.contains(O))
			return O->Repeat(Times);
		else
			return UID::Exception_InvalidObject; }, UID::Port_ObjectRepeat);
	BindFunctionToPort([](Object *O)
					   {
		if(AllObjects.contains(O))
			return O->Pause();
		else
			return UID::Exception_InvalidObject; }, UID::Port_ObjectPause);
	BindFunctionToPort([](Object *O)
					   {
		if(AllObjects.contains(O))
			return O->Continue();
		else
			return UID::Exception_InvalidObject; }, UID::Port_ObjectContinue);
	BindFunctionToPort([](Object *O)
					   {
		if(AllObjects.contains(O))
			return O->Abort();
		else
			return UID::Exception_InvalidObject; }, UID::Port_ObjectAbort);
	BindFunctionToPort([](Object *O, uint8_t ToPort)
					   {
		if(AllObjects.contains(O))
		{
			std::vector<char>Information;
			O->GetInformation(Information);
			Async_stream_IO::Send(std::move(Information),ToPort);
			return UID::Exception_Success;
		}
		else
			return UID::Exception_InvalidObject; }, UID::Port_ObjectGetInformation);
	BindFunctionToPort([](Object *O)
					   {
		if(AllObjects.erase(O))
		{
			delete O;
			return UID::Exception_Success;
		}
		else
			return UID::Exception_InvalidObject; }, UID::Port_ObjectDestroy);
	BindFunctionToPort([](uint8_t ToPort)
					   {
		std::dynarray<char> Message(sizeof(Object*)*AllObjects.size()+sizeof(uint8_t));
		*reinterpret_cast<uint8_t*>(Message.data())=AllObjects.size();
		std::copy(AllObjects.cbegin(),AllObjects.cend(),reinterpret_cast<Object**>(Message.data()+sizeof(uint8_t)));
		Async_stream_IO::Send(std::move(Message),ToPort); },
					   UID::Port_AllObjects);
#ifdef ARDUINO_ARCH_AVR
	Async_stream_IO::RemoteInvoke(static_cast<uint8_t>(UID::Port_RandomSeed), [](Async_stream_IO::Exception, uint32_t RandomSeed)
								  { std::ArduinoUrng::seed(RandomSeed); });
#endif
}
void loop()
{
	Async_stream_IO::ExecuteTransactionsInQueue();
}