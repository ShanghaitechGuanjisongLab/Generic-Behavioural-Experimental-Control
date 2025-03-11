#include "Objects.h"
#include <set>
#include <sstream>
template <typename T>
inline void BindFunctionToPort(T&& Function, UID Port)
{
	Async_stream_IO::BindFunctionToPort(std::forward<T>(Function), static_cast<uint8_t>(Port));
}
// 此全局对象不上锁，禁止使用中断处理方法访问
std::set<RootObject*> AllObjects;
std::unordered_map<UID, ChildObject*>AsyncObjects;
// 此全局对象可以被中断处理方法访问，因此在中断启用时禁止访问。从此容器中删除的任务，仍可能会被执行最后一次，因此任务的资源应当由任务本身负责释放
std::set<const std::shared_ptr<const std::move_only_function<void() const>>> IdleTasks;
extern const std::unordered_map<UID, RootObject* (*)()> ObjectCreators;
ArchUrng Urng;
void setup()
{
	Serial.setTimeout(-1);
	Serial.begin(9600);
	BindFunctionToPort([]()
		{ return static_cast<uint8_t>(sizeof(size_t)); }, UID::Port_PointerSize);
	BindFunctionToPort([](UID Subclass, uint8_t ProgressPort)
		{
			const auto Iterator = ObjectCreators.find(Subclass);
			if (Iterator == ObjectCreators.end())
				return static_cast<RootObject*>(nullptr);
			RootObject* const NewObject = Iterator->second();
			AllObjects.insert(NewObject);
			return NewObject; }, UID::Port_ObjectCreate);
	BindFunctionToPort([](RootObject* O)
		{
			if (AllObjects.contains(O))
			{
				noInterrupts();
				const UID Result = O->Start();
				interrupts();
				return Result;
			}
			else
				return UID::Exception_InvalidObject; }, UID::Port_ObjectStart);
	Async_stream_IO::Listen([](const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize)
		{
			static std::vector<char> Message;
			Message.resize(MessageSize);
			MessageReader(Message.data(), MessageSize);
#pragma pack(push, 1)
			const struct RestoreHeader
			{
				uint8_t RemotePort;
				RootObject* O;
				char* ProgressInfo() const { return (char*)(this + 1); }
			} &Arguments = *reinterpret_cast<RestoreHeader*>(Message.data());
#pragma pack(pop)
			if (AllObjects.contains(Arguments.O))
			{
				noInterrupts();
				Async_stream_IO::Send(Arguments.O->Restore(std::span<const char>(Arguments.ProgressInfo(), MessageSize - sizeof(RestoreHeader))), Arguments.RemotePort);
				interrupts();
			}
			else
				Async_stream_IO::Send(UID::Exception_InvalidObject, Arguments.RemotePort);
		}, static_cast<uint8_t>(UID::Port_ObjectRestore));
	BindFunctionToPort([](RootObject* O, uint16_t Times)
		{
			if (AllObjects.contains(O))
			{
				noInterrupts();
				const UID Result = O->Repeat(Times);
				interrupts();
				return Result;
			}
			else
				return UID::Exception_InvalidObject; }, UID::Port_ObjectRepeat);
	BindFunctionToPort([](RootObject* O)
		{
			if (AllObjects.contains(O))
			{
				noInterrupts();
				const UID Result = O->Pause();
				interrupts();
				return Result;
			}
			else
				return UID::Exception_InvalidObject; }, UID::Port_ObjectPause);
	BindFunctionToPort([](RootObject* O)
		{
			if (AllObjects.contains(O))
			{
				noInterrupts();
				const UID Result = O->Continue();
				interrupts();
				return Result;
			}
			else
				return UID::Exception_InvalidObject; }, UID::Port_ObjectContinue);
	BindFunctionToPort([](RootObject* O)
		{
			if (AllObjects.contains(O))
			{
				noInterrupts();
				const UID Result = O->Abort();
				interrupts();
				return Result;
			}
			else
				return UID::Exception_InvalidObject; }, UID::Port_ObjectAbort);
	BindFunctionToPort([](RootObject* O, uint8_t RemotePort)
		{
			if (AllObjects.contains(O))
			{
				std::dynarray<char> Information;
				noInterrupts();
				const UID Result = O->GetInformation(Information);
				interrupts();
				if (Result == UID::Exception_Success)
					//不能将O传入Send等实际发送时再获取信息，因为那时O可能已经被销毁
					Async_stream_IO::Send([Information = std::move(Information)](const std::move_only_function<void(const void* Message, uint8_t Size) const>& MessageSender)
						{
							MessageSender(Information.data(), Information.size());
						}, RemotePort);
				return Result;
			}
			else
				return UID::Exception_InvalidObject; }, UID::Port_ObjectGetInformation);
	BindFunctionToPort([](RootObject* O)
		{
			if (AllObjects.erase(O))
			{
				noInterrupts();
				delete O;
				interrupts();
				return UID::Exception_Success;
			}
			else
				return UID::Exception_InvalidObject; }, UID::Port_ObjectDestroy);
	BindFunctionToPort([](uint8_t ToPort)
		{ Async_stream_IO::Send([](const std::move_only_function<void(const void* Message, uint8_t Size) const>& MessageSender)
			{
				const size_t MessageSize = sizeof(RootObject*) * AllObjects.size() + sizeof(uint8_t);
				std::unique_ptr<uint8_t[]>Message = std::make_unique_for_overwrite<uint8_t[]>(MessageSize);
				Message[0] = AllObjects.size();
				std::copy(AllObjects.cbegin(), AllObjects.cend(), reinterpret_cast<RootObject**>(Message.get() + 1));
				MessageSender(Message.get(), MessageSize); }, ToPort); },
		UID::Port_AllObjects);
#ifdef ARDUINO_ARCH_AVR
	Async_stream_IO::RemoteInvoke(static_cast<uint8_t>(UID::Port_RandomSeed), [](Async_stream_IO::Exception, uint32_t RandomSeed)
		{ std::ArduinoUrng::seed(RandomSeed); });
#endif
}
void loop()
{
	Async_stream_IO::ExecuteTransactionsInQueue();
	noInterrupts();
	// 必须拷贝一份，因为IdleTasks可能在中断中被修改，这种修改可能是添加，也可能是删除
	const std::set<const std::shared_ptr<const std::move_only_function<void() const>>> IdleTasksCopy(IdleTasks);
	interrupts();
	for (const std::shared_ptr<const std::move_only_function<void() const>>& Task : IdleTasksCopy)
		(*Task)();
}
#include <TimersOneForAll_Define.hpp>