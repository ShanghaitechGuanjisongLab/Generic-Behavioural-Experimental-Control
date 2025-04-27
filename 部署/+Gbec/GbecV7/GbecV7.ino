#include "ExperimentDesign.hpp"
#include <sstream>
#include <map>
#include <vector>

static std::move_only_function<void()const> const FinishCallback = []()
{
	Async_stream_IO::Send(UID::Signal_ObjectFinished, static_cast<uint8_t>(UID::Port_Signal));
};

template <typename T>
struct StepConstructors;
template <typename... Pairs>
struct StepConstructors<std::tuple<Pairs...>>
{
	static std::unordered_map<UID, Step *(*)()> value;
};
template <typename... Pairs>
std::unordered_map<UID, Step *(*)()> StepConstructors<std::tuple<Pairs...>>::value{{Pairs::ID, []()
																					{ return new typename Pairs::StepType(FinishCallback); }...}};
static std::set<Step *> RootSteps;

template <typename T>
inline void BindFunctionToPort(T &&Function, UID Port)
{
	Async_stream_IO::BindFunctionToPort(std::forward<T>(Function), static_cast<uint8_t>(Port));
}

ArchUrng Urng;
std::move_only_function<void() const> const NullCallback = []() {};
void setup()
{
	Serial.setTimeout(-1);
	Serial.begin(9600);
	BindFunctionToPort([]()
					   { return static_cast<uint8_t>(sizeof(size_t)); }, UID::Port_PointerSize);
	BindFunctionToPort([](UID Subclass, uint8_t ProgressPort)
					   {
			const auto Iterator = StepConstructors<PublicSteps>::value.find(Subclass);
			if (Iterator == StepConstructors<PublicSteps>::value.end())
				return static_cast<Step*>(nullptr);
			Step*const NewStep = Iterator->second();
			RootSteps.insert(NewStep);
			return NewStep; }, UID::Port_ObjectCreate);
	BindFunctionToPort([](Step *O)
					   {
			if (RootSteps.contains(O))
			{
				if(O->Start())
				FinishCallback();
				return UID::Signal_ObjectStarted;
			}
			else
				return UID::Exception_InvalidObject; }, UID::Port_ObjectStart);
	Async_stream_IO::Listen([](const std::move_only_function<void(void *Message, uint8_t Size) const> &MessageReader, uint8_t MessageSize)
							{
			static std::vector<char> Message;
			Message.resize(MessageSize);
			MessageReader(Message.data(), MessageSize);
#pragma pack(push, 1)
			const struct RestoreHeader
			{
				uint8_t RemotePort;
				Step* O;
				char* ProgressInfo() const { return (char*)(this + 1); }
			} &Arguments = *reinterpret_cast<RestoreHeader*>(Message.data());
#pragma pack(pop)
			if (RootSteps.contains(Arguments.O))
			{
				noInterrupts();
				Async_stream_IO::Send(Arguments.O->Restore(std::span<const char>(Arguments.ProgressInfo(), MessageSize - sizeof(RestoreHeader))), Arguments.RemotePort);
				interrupts();
			}
			else
				Async_stream_IO::Send(UID::Exception_InvalidObject, Arguments.RemotePort); }, static_cast<uint8_t>(UID::Port_ObjectRestore));
	BindFunctionToPort([](Step *O, uint16_t Times)
					   {
			if (RootSteps.contains(O))
			{
				noInterrupts();
				const UID Result = O->Repeat(Times);
				interrupts();
				return Result;
			}
			else
				return UID::Exception_InvalidObject; }, UID::Port_ObjectRepeat);
	BindFunctionToPort([](Step *O)
					   {
			if (RootSteps.contains(O))
			{
				noInterrupts();
				const UID Result = O->Pause();
				interrupts();
				return Result;
			}
			else
				return UID::Exception_InvalidObject; }, UID::Port_ObjectPause);
	BindFunctionToPort([](Step *O)
					   {
			if (RootSteps.contains(O))
			{
				noInterrupts();
				const UID Result = O->Continue();
				interrupts();
				return Result;
			}
			else
				return UID::Exception_InvalidObject; }, UID::Port_ObjectContinue);
	BindFunctionToPort([](Step *O)
					   {
			if (RootSteps.contains(O))
			{
				noInterrupts();
				const UID Result = O->Abort();
				interrupts();
				return Result;
			}
			else
				return UID::Exception_InvalidObject; }, UID::Port_ObjectAbort);
	BindFunctionToPort([](Step *O, uint8_t RemotePort)
					   {
			if (RootSteps.contains(O))
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
	BindFunctionToPort([](Step *O)
					   {
			if (RootSteps.erase(O))
			{
				noInterrupts();
				delete O;
				interrupts();
				return UID::Exception_Success;
			}
			else
				return UID::Exception_InvalidObject; }, UID::Port_ObjectDestroy);
	BindFunctionToPort([](uint8_t ToPort)
					   { Async_stream_IO::Send([](const std::move_only_function<void(const void *Message, uint8_t Size) const> &MessageSender)
											   {
				const size_t MessageSize = sizeof(Step*) * RootSteps.size() + sizeof(uint8_t);
				std::unique_ptr<uint8_t[]>Message = std::make_unique_for_overwrite<uint8_t[]>(MessageSize);
				Message[0] = RootSteps.size();
				std::copy(RootSteps.cbegin(), RootSteps.cend(), reinterpret_cast<Step**>(Message.get() + 1));
				MessageSender(Message.get(), MessageSize); }, ToPort); },
					   UID::Port_AllObjects);
#ifdef ARDUINO_ARCH_AVR
	Async_stream_IO::RemoteInvoke(static_cast<uint8_t>(UID::Port_RandomSeed), [](Async_stream_IO::Exception, uint32_t RandomSeed)
								  { std::ArduinoUrng::seed(RandomSeed); });
#endif
}
std::set<std::move_only_function<void() const> const *> _PendingInterrupts;
void loop()
{
	noInterrupts();
	for (auto M : _PendingInterrupts)
		M->operator()();
	_PendingInterrupts.clear();
	interrupts();
	Async_stream_IO::ExecuteTransactionsInQueue();
}
#include <TimersOneForAll_Define.hpp>