#include "Objects.h"
#include "Async_stream_IO.hpp"
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
	BindFunctionToPort([](Object*O)
	{
		if(AllObjects.con)
	}, UID::Port_ObjectPause);
	BindFunctionToPort(Object::Continue, UID::Port_ObjectContinue);
	BindFunctionToPort(Object::Stop, UID::Port_ObjectStop);
	BindFunctionToPort(Object::GetInformation, UID::Port_ObjectGetInformation);
	BindFunctionToPort(Object::Destroy, UID::Port_ObjectDestroy);
	BindFunctionToPort(Object::AllObjects, UID::Port_AllObjects);
}
void loop()
{
	Async_stream_IO::ExecuteTransactionsInQueue();
}