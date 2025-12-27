#pragma once
#include "UID.hpp"
#include "Async_stream_IO.hpp"
#include "Timers_one_for_all.hpp"
#include <Quick_digital_IO_interrupt.hpp>
#include <map>
#include <random>
#include <sstream>
#include <functional>
#include <queue>
#include <set>
#include <iterator>
using namespace std::chrono_literals;
using DurationRep = uint32_t;
struct PinListener {
	uint8_t const Pin;
	std::move_only_function<void()> Callback;
	// 中断不安全
	void Pause() {
		std::set<std::move_only_function<void()> *> *CallbackSet = &Listening[Pin];
		CallbackSet->erase(&Callback);
		if (CallbackSet->empty())
			Quick_digital_IO_interrupt::DetachInterrupt(Pin);
		CallbackSet = &Resting[Pin];
		CallbackSet->erase(&Callback);
		if (CallbackSet->empty())
			Resting.erase(Pin);
	}
	// 中断不安全
	void Continue() {
		std::set<std::move_only_function<void()> *> &CallbackSet = Listening[Pin];
		if (CallbackSet.empty())
			Quick_digital_IO_interrupt::AttachInterrupt<RISING>(Pin, PinInterrupt{ Pin });
		CallbackSet.insert(&Callback);
	}
	// 中断不安全
	PinListener(uint8_t Pin, std::move_only_function<void()> &&Callback)
	  : Pin(Pin), Callback(std::move(Callback)) {
		Continue();
	}
	// 中断不安全
	~PinListener() {
		Pause();
	}
	// 中断安全
	static void ClearPending() {
		static std::queue<std::move_only_function<void()> *> LocalSwap;
		{
			Quick_digital_IO_interrupt::InterruptGuard const _;
			std::swap(LocalSwap, PendingCallbacks);
			for (auto &Iterator : Resting) {
				std::set<std::move_only_function<void()> *> &ListeningSet = Listening[Iterator.first];
				ListeningSet.merge(Iterator.second);
				if (ListeningSet.size())
					Quick_digital_IO_interrupt::AttachInterrupt<RISING>(Iterator.first, PinInterrupt{ Iterator.first });
				Iterator.second.clear();
			}
		}
		while (LocalSwap.size()) {
			(*LocalSwap.front())();
			LocalSwap.pop();
		}
	}

protected:
	static std::queue<std::move_only_function<void()> *> PendingCallbacks;
	static std::unordered_map<uint8_t, std::set<std::move_only_function<void()> *>> Listening;
	static std::unordered_map<uint8_t, std::set<std::move_only_function<void()> *>> Resting;
	struct PinInterrupt {
		uint8_t const Pin;
		void operator()() const {
			std::set<std::move_only_function<void()> *> &Listenings = Listening[Pin];
			for (auto H : Listenings)
				PendingCallbacks.push(H);
			Resting[Pin].merge(Listenings);
			Listenings.clear();
			Quick_digital_IO_interrupt::DetachInterrupt(Pin);
		}
	};
};

inline static void InfoWrite(std::ostringstream &InfoStream, UID InfoValue) {
	InfoStream.put(static_cast<char>(InfoValue));
}
inline static void InfoWrite(std::ostringstream &InfoStream, uint8_t InfoValue) {
	InfoStream.put(static_cast<char>(InfoValue));
}
template<typename T>
inline static void InfoWrite(std::ostringstream &InfoStream, T InfoValue) {
	InfoStream.write(reinterpret_cast<const char *>(&InfoValue), sizeof(InfoValue));
}
// 强制要求2个以上输入，以免递归
template<typename T1, typename T2, typename... T>
inline static void InfoWrite(std::ostringstream &InfoStream, T1 InfoValue1, T2 InfoValue2, T... InfoValues) {
	int _[] = { (InfoWrite(InfoStream, InfoValue1), 0), (InfoWrite(InfoStream, InfoValue2), 0), (InfoWrite(InfoStream, InfoValues), 0)... };
}
extern Async_stream_IO::AsyncStream SerialStream;

template<typename Target>
struct _ModuleID {
	static UID const &ID;
};
struct Process;
struct IInformative {
	virtual void WriteInfo() const = 0;
	// 使用函数而非成员变量以节省运行内存
	virtual Async_stream_IO::MessageSize InfoSize() const = 0;
	virtual ~IInformative() = default;
};
// 所有模块的基类，本身可以当作一个什么都不做的空模块使用
struct Module : IInformative {
	Process &Container;
	constexpr Module(Process &Container)
	  : Container(Container) {
	}
	// 返回是否需要等待回调，并提供回调函数。返回true表示模块还在执行中，将在执行完毕后调用回调函数；返回false表示模块已执行完毕，不会调用回调函数。
	virtual bool Start(std::move_only_function<void()> &FinishCallback) {
		return false;
	}
	// 放弃该步骤。未在执行的步骤放弃也不会出错。
	virtual void Abort() {}
	// 重新开始当前执行中的步骤，不改变下一步。不应试图重启当前未在执行中的步骤。
	virtual void Restart() {}

	// 注意模块析构时不Abort。模块的Abort只由ModuleAbort步骤负责调用。
	static constexpr uint16_t NumTrials = 0;

protected:
	static std::move_only_function<void()> _EmptyCallback;
	struct _EmptyStart {
		Module *const ContentModule;
		void operator()() const {
			ContentModule->Start(_EmptyCallback);
		}
	};
};

// 仅在LoadModule时用作重载提示，无定义
template<UID ID>
struct IDModule;
template<typename M>
struct _IDModule {
	using type = M;
};
template<typename M>
using _IDModule_t = typename _IDModule<M>::type;

template<typename T>
struct _TypeID {
	static constexpr UID value = UID::Type_Pointer;
};
template<>
struct _TypeID<std::chrono::seconds> {
	static constexpr UID value = UID::Type_Seconds;
};
template<>
struct _TypeID<std::chrono::milliseconds> {
	static constexpr UID value = UID::Type_Milliseconds;
};
template<>
struct _TypeID<std::chrono::microseconds> {
	static constexpr UID value = UID::Type_Microseconds;
};
template<>
struct _TypeID<UID> {
	static constexpr UID value = UID::Type_UID;
};
template<>
struct _TypeID<uint8_t> {
	static constexpr UID value = UID::Type_UInt8;
};
template<>
struct _TypeID<uint16_t> {
	static constexpr UID value = UID::Type_UInt16;
};
template<>
struct _TypeID<uint32_t> {
	static constexpr UID value = UID::Type_UInt32;
};
template<>
struct _TypeID<bool> {
	static constexpr UID value = UID::Type_Bool;
};
#pragma pack(push, 1)
template<typename T>
struct PodField {
	UID const FieldName;
	UID const FieldType;
	T FieldValue;
	constexpr PodField(UID FieldName, T FieldValue)
	  : FieldName(FieldName), FieldType(_TypeID<T>::value), FieldValue(FieldValue) {
	}
	constexpr PodField(UID FieldName, UID FieldType, T FieldValue)
	  : FieldName(FieldName), FieldType(FieldType), FieldValue(FieldValue) {
	}
};
#pragma pack(pop)
class Process {
	Async_stream_IO::MessageSize InfoSize;
	std::set<PinListener *> ActiveInterrupts;
	std::set<Timers_one_for_all::TimerClass *> ActiveTimers;
	std::map<UID const *, std::unique_ptr<IInformative>> Modules;
	uint16_t TimesLeft;
	UID const *StartPointer;
#pragma pack(push, 1)
	struct InfoHeader {
		uint8_t const NumFields = 2;
		PodField<UID const *> StartModule;
		UID const Field2Name = UID::Field_Modules;
		UID const Field2Type = UID::Type_Map;
		uint8_t NumModules;
		UID const ModuleKeyType = UID::Type_Pointer;
		UID const ModuleValueType = UID::Type_Struct;
		constexpr InfoHeader(UID const *StartPointer, uint8_t NumModules)
		  : StartModule{ UID::Field_StartModule, StartPointer }, NumModules(NumModules) {
		}
	};
#pragma pack(pop)
	std::move_only_function<void()> FinishCallback{
		[this]() {
		  while (--TimesLeft)
			  // 基类指针转派生，不能用reinterpret_cast
			  if (static_cast<Module *>(Modules[StartPointer].get())->Start(FinishCallback))
				  return;
		  SerialStream.AsyncInvoke(static_cast<Async_stream_IO::Port>(UID::PortC_ProcessFinished), this);
		}
	};

	// 中断不安全
	void _Abort() {
		for (PinListener *H : ActiveInterrupts)
			delete H;
		for (Timers_one_for_all::TimerClass *T : ActiveTimers) {
			T->Stop();
			T->Allocatable = true;  // 使其可以被重新分配
		}
		for (std::move_only_function<void()> *Cleaner : ExtraCleaners)
			(*Cleaner)();
	}
	template<typename T>
	auto Construct(T *At) -> decltype(new (At) T(*this)) {
		new (At) T(*this);
	}
	template<typename T>
	static auto Construct(T *At) -> decltype(new (At) T()) {
		new (At) T;
	}

public:
	void Pause() const {
		for (PinListener *H : ActiveInterrupts)
			H->Pause();
		for (Timers_one_for_all::TimerClass *T : ActiveTimers)
			T->Pause();
	}
	void Continue() const {
		for (PinListener *H : ActiveInterrupts)
			H->Continue();
		for (Timers_one_for_all::TimerClass *T : ActiveTimers)
			T->Continue();
	}
	void Abort() {
		_Abort();
		ActiveInterrupts.clear();
		ActiveTimers.clear();
		TrialsDone.clear();
		ExtraCleaners.clear();
	}
	virtual ~Process() {
		_Abort();
	}
	PinListener *RegisterInterrupt(uint8_t Pin, std::move_only_function<void()> &&Callback) {
		PinListener *Listener = new PinListener(Pin, std::move(Callback));
		ActiveInterrupts.insert(Listener);
		return Listener;
	}
	void UnregisterInterrupt(PinListener *Handle) {
		delete Handle;
		ActiveInterrupts.erase(Handle);
	}
	Timers_one_for_all::TimerClass *AllocateTimer() {
		Timers_one_for_all::TimerClass *Timer = Timers_one_for_all::AllocateTimer();
		ActiveTimers.insert(Timer);
		return Timer;
	}
	// 此操作不负责停止计时器，仅使其可以被再分配
	void UnregisterTimer(Timers_one_for_all::TimerClass *Timer) {
		ActiveTimers.erase(Timer);
		Timer->Allocatable = true;
	}
	template<typename ModuleType>
	_IDModule_t<ModuleType> *LoadModule() {
		using _ModuleType = _IDModule_t<ModuleType>;
		auto const Iter = Modules.find(&_ModuleID<_ModuleType>::ID);
		if (Iter != Modules.end())
			return static_cast<_ModuleType *>(Iter->second.get());
		_ModuleType *const RawMemory = static_cast<_ModuleType *>(operator new(sizeof(_ModuleType)));
		Modules.emplace(&_ModuleID<_ModuleType>::ID, std::unique_ptr<IInformative>(RawMemory));
		Construct(RawMemory);
		InfoSize += RawMemory->InfoSize() + sizeof(decltype(Modules)::key_type);
		return RawMemory;
	}

	// 此方法会终止并清空当前执行的所有模块（通过清理资源的方法，不调用模块Abort，但会调用清理模块），然后再开始新的模块
	template<typename Entry>
	uint16_t LoadStartModule() {
		using _Entry = _IDModule_t<Entry>;
		Abort();
		Modules.clear();
		InfoSize = sizeof(InfoHeader);
		StartPointer = &_ModuleID<_Entry>::ID;
		LoadModule<_Entry>();
		return _Entry::NumTrials;
	}

	bool Start(uint16_t Times) {
		for (TimesLeft = Times; TimesLeft > 0; --TimesLeft)
			if (static_cast<Module *>(Modules[StartPointer].get())->Start(FinishCallback))
				return true;
		return false;
	}
	// 发送当前或上一个执行模块及其关联模块的所有信息
	void SendInfo(Async_stream_IO::Port Port) const {
		Async_stream_IO::InterruptGuard const _ = SerialStream.BeginSend(InfoSize, Port);
		SerialStream << InfoHeader(StartPointer, Modules.size());
		for (auto const &Iterator : Modules) {
			SerialStream << Iterator.first;
			Iterator.second->WriteInfo();
		}
	}
	std::unordered_map<UID, uint16_t> TrialsDone;
	std::set<std::move_only_function<void()> *> ExtraCleaners;
};
struct IRandom {
	virtual void Randomize() = 0;
};

template<uint16_t...>
struct _Sum {
	static constexpr uint16_t value = 0;
};
template<uint16_t First, uint16_t... Rest>
struct _Sum<First, Rest...> {
	static constexpr uint16_t value = First + _Sum<Rest...>::value;
};

#define InfoImplement \
	static UID const ID; \
	void WriteInfo() const override { \
		SerialStream << InfoStruct{}; \
	} \
	Async_stream_IO::MessageSize InfoSize() const override { \
		return sizeof(InfoStruct); \
	}
template<typename... SubModules>
struct RandomSequential : Module, IRandom {
protected:
	static constexpr
#ifdef ARDUINO_ARCH_AVR
	  std::ArduinoUrng
#endif
#ifdef ARDUINO_ARCH_SAM
	    std::TrueUrng
#endif
	      Urng{};
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 2;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<RandomSequential>::ID };
		UID const Field2Name = UID::Field_Modules;
		UID const Field2Type = UID::Type_Array;
		uint8_t const NumModules{ sizeof...(SubModules) };
		UID const ModuleType = UID::Type_Pointer;
		UID const *const ModuleValues[sizeof...(SubModules)] = { &_ModuleID<_IDModule_t<SubModules>>::ID... };
	};
#pragma pack(pop)
	Module *SubPointers[sizeof...(SubModules)] = { Module::Container.LoadModule<_IDModule_t<SubModules>>()... };  // SAM编译器不能自动推断数组长度，导致cend不可用
	Module *const *CurrentModule = std::cend(SubPointers);
	std::move_only_function<void()> NextBlock;

public:
	template<uint16_t... Repeats>
	struct WithRepeat : Module, IRandom {
	protected:
#pragma pack(push, 1)
		struct InfoStruct {
			uint8_t const NumFields = 2;
			PodField<UID> const ID{ UID::Field_ID, _ModuleID<WithRepeat>::ID };
			UID const Field2Name = UID::Field_Modules;
			UID const Field2Type = UID::Type_Table;
			uint8_t const NumModules{ sizeof...(SubModules) };
			uint8_t const NumColumns = 2;
			UID const Column1Name = UID::Column_Module;
			UID const Column1Type = UID::Type_Pointer;
			UID const *const Column1Values[sizeof...(SubModules)] = { &_ModuleID<_IDModule_t<SubModules>>::ID... };
			UID const Column2Name = UID::Column_Repeats;
			UID const Column2Type = UID::Type_UInt16;
			uint16_t const Column2Values[sizeof...(SubModules)];
			constexpr InfoStruct()
			  : Column2Values{ Repeats... } {}
		};
#pragma pack(pop)
		Module *SubPointers[_Sum<Repeats...>::value];
		Module **CurrentModule = std::begin(SubPointers);
		std::move_only_function<void()> NextBlock;  // 不能在此处等号初始化，SAM会编译器错误
	public:
		void Randomize() override {
			std::shuffle(std::begin(SubPointers), std::end(SubPointers), Urng);
		}
		WithRepeat(Process &Container)
		  : Module(Container), NextBlock{ [this]() {
			    while (++CurrentModule < std::end(SubPointers))
				    if ((*CurrentModule)->Start(NextBlock))
					    return;
			  } } {
			Module **_[] = { (CurrentModule = std::fill_n(CurrentModule, Repeats, Module::Container.LoadModule<_IDModule_t<SubModules>>()))... };
			Randomize();
		}
		void Abort() override {
			if (CurrentModule < std::end(SubPointers))
				(*CurrentModule)->Abort();
		}
		void Restart() override {
			Abort();
			for (CurrentModule = std::begin(SubPointers); CurrentModule < std::end(SubPointers); ++CurrentModule)
				if ((*CurrentModule)->Start(NextBlock))
					return;
		}
		bool Start(std::move_only_function<void()> &FC) override {
			Abort();
			for (CurrentModule = std::begin(SubPointers); CurrentModule < std::end(SubPointers); ++CurrentModule)
				if ((*CurrentModule)->Start(NextBlock)) {
					NextBlock = [this, &FC]() {
						while (++CurrentModule < std::end(SubPointers))
							if ((*CurrentModule)->Start(NextBlock))
								return;
						FC();
					};
					return true;
				}
			return false;
		}
		static constexpr uint16_t NumTrials = _Sum<_IDModule_t<SubModules>::NumTrials * Repeats...>::value;
		InfoImplement;
	};
	void Randomize() override {
		std::shuffle(std::begin(SubPointers), std::end(SubPointers), Urng);
	}
	RandomSequential(Process &Container)
	  : Module(Container), NextBlock{ [this]() {
		    while (++CurrentModule < std::cend(SubPointers))
			    if ((*CurrentModule)->Start(NextBlock))
				    return;
		  } } {
		Randomize();
	}
	void Abort() override {
		if (CurrentModule < std::cend(SubPointers))
			(*CurrentModule)->Abort();
	}
	void Restart() override {
		Abort();
		for (CurrentModule = std::cbegin(SubPointers); CurrentModule < std::cend(SubPointers); ++CurrentModule)
			if ((*CurrentModule)->Start(NextBlock))
				return;
	}
	bool Start(std::move_only_function<void()> &FC) override {
		Abort();
		for (CurrentModule = std::cbegin(SubPointers); CurrentModule < std::cend(SubPointers); ++CurrentModule)
			if ((*CurrentModule)->Start(NextBlock)) {
				NextBlock = [this, &FC]() {
					while (++CurrentModule < std::cend(SubPointers))
						if ((*CurrentModule)->Start(NextBlock))
							return;
					FC();
				};
				return true;
			}
		return false;
	}
	static constexpr uint16_t NumTrials = _Sum<_IDModule_t<SubModules>::NumTrials...>::value;
	InfoImplement;
};
template<typename... SubModules>
UID const RandomSequential<SubModules...>::ID = UID::Module_RandomSequential;
template<typename... SubModules>
template<uint16_t... Repeats>
UID const RandomSequential<SubModules...>::WithRepeat<Repeats...>::ID = UID::Module_RandomSequential;
// 表示一个固定整数
template<DurationRep Value>
struct ConstantInteger : IInformative {
protected:
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 2;
		PodField<UID> const ID{ UID::Field_ID, UID::Module_ConstantInteger };
		PodField<DurationRep> const ValueField{ UID::Field_Value, Value };
	};
#pragma pack(pop)
public:
	static constexpr uint16_t NumTrials = Value;
	constexpr DurationRep Current() const {
		return Value;
	}
	InfoImplement;
};
template<DurationRep Value>
UID const ConstantInteger<Value>::ID = UID::Module_ConstantInteger;
// 表示一个随机整数。该步骤维护一个随机变量，只有使用Randomize步骤才能改变这个变量，否则一直保持相同的值。如果有多个随机范围相同的随机变量需要独立控制随机性，可以指定不同的ID，否则视为同一个随机变量。
template<DurationRep Min, DurationRep Max, UID CustomID = UID::Module_RandomInteger>
struct RandomInteger : IInformative, IRandom {
protected:
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 2;
		PodField<UID> const ID{ UID::Field_ID, CustomID };
		UID const Field2Name = UID::Field_Range;
		UID const Field2Type = UID::Type_Array;
		uint8_t const NumElements = 2;
		UID const ElementType = _TypeID<DurationRep>::value;
		DurationRep const MinValue = Min;
		DurationRep const MaxValue = Max;
	};
#pragma pack(pop)
	DurationRep _Current;

public:
	RandomInteger() {
		Randomize();
	}
	void Randomize() {
		constexpr double DoubleMin = Min;
		_Current = static_cast<DurationRep>(pow(static_cast<double>(Max) / DoubleMin, static_cast<double>(random(__LONG_MAX__)) / (__LONG_MAX__ - 1)) * DoubleMin);
	}
	static constexpr uint16_t NumTrials = 0;
	DurationRep Current() const {
		return _Current;
	}
	InfoImplement;
};
template<DurationRep Min, DurationRep Max, UID CustomID>
UID const RandomInteger<Min, Max, CustomID>::ID = CustomID;
using Infinite = void;
// 无限重复模块，也可以额外指定UntilTimes重复次数。不能指定重复时间，请组合使用 Async Delay ModuleAbort 等模块实现。
template<typename Content, typename Times = Infinite>
struct Repeat : Module {
protected:
	uint16_t TimesLeft;
	Times const *const T;
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 3;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<Repeat>::ID };
		PodField<UID const *> const ContentField{ UID::Field_Content, &_ModuleID<_IDModule_t<Content>>::ID };
		PodField<UID const *> const TimesField{ UID::Field_Times, &_ModuleID<Times>::ID };
	};
#pragma pack(pop)
	_IDModule_t<Content> *const ContentPtr = Module::Container.LoadModule<Content>();
	std::move_only_function<void()> NextBlock;

public:
	Repeat(Process &Container)
	  : Module(Container), NextBlock{ [this]() {
		    while (--TimesLeft)
			    if (ContentPtr->Start(NextBlock))
				    return;
		  } },
	    T{ Module::Container.LoadModule<Times>() } {
	}
	void Restart() override {
		Abort();
		for (TimesLeft = T->Current(); TimesLeft; --TimesLeft)
			if (ContentPtr->Start(NextBlock))
				return;
	}
	bool Start(std::move_only_function<void()> &FC) override {
		Abort();
		for (TimesLeft = T->Current(); TimesLeft; --TimesLeft)
			if (ContentPtr->Start(NextBlock)) {
				NextBlock = [this, &FC]() {
					while (--TimesLeft)
						if (ContentPtr->Start(NextBlock))
							return;
					FC();
				};
				return true;
			}
		return false;
	}
	static constexpr uint16_t NumTrials = _IDModule_t<Content>::NumTrials * Times::NumTrials;
	InfoImplement;
};
template<typename Content, typename Times>
UID const Repeat<Content, Times>::ID = UID::Module_Repeat;
template<typename Content>
struct Repeat<Content, Infinite> : Module {
protected:
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 2;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<Repeat>::ID };
		PodField<UID const *> const ContentField{ UID::Field_Content, &_ModuleID<_IDModule_t<Content>>::ID };
	};
#pragma pack(pop)
	_IDModule_t<Content> *const ContentPtr = Module::Container.LoadModule<Content>();
	std::move_only_function<void()> NextBlock;
public:
	Repeat(Process &Container)
	  : Module(Container), NextBlock{ [this]() {
		    for (;;)
			    if (ContentPtr->Start(NextBlock))
				    return;
		  } } {
	}
	void Abort() override {
		ContentPtr->Abort();
	}
	void Restart() override {
		ContentPtr->Abort();
		NextBlock();
	}
	bool Start(std::move_only_function<void()> &FC) override {
		Abort();
		for (;;)
			if (ContentPtr->Start(NextBlock))
				return true;
		return false;
	}
	static constexpr uint16_t NumTrials = _IDModule_t<Content>::NumTrials;
	InfoImplement;
};
template<typename Content>
UID const Repeat<Content, Infinite>::ID = UID::Module_Repeat;
template<typename... SubModules>
struct _Sequential : Module {
protected:
	Module *const SubPointers[sizeof...(SubModules)] = { Module::Container.LoadModule<SubModules>()... };
	Module *const *CurrentModule = std::cend(SubPointers);

	// 必须预设NextBlock，这样即使没有Start直接Restart也能执行完整个模块
	std::move_only_function<void()> NextBlock;
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 2;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<_Sequential>::ID };
		UID const Field2Name = UID::Field_Modules;
		UID const Field2Type = UID::Type_Array;
		uint8_t const NumModules{ sizeof...(SubModules) };
		UID const ModuleType = UID::Type_Pointer;
		UID const *const ModuleValues[sizeof...(SubModules)] = { &_ModuleID<_IDModule_t<SubModules>>::ID... };
	};
#pragma pack(pop)
public:
	_Sequential(Process &Container)
	  : Module(Container), NextBlock{ [this]() {
		    while (++CurrentModule < std::cend(SubPointers))
			    if ((*CurrentModule)->Start(NextBlock))
				    return;
		  } } {
	}
	void Abort() override {
		if (CurrentModule < std::cend(SubPointers))
			(*CurrentModule)->Abort();
	}
	void Restart() override {
		Abort();
		for (CurrentModule = std::cbegin(SubPointers); CurrentModule < std::cend(SubPointers); ++CurrentModule)
			if ((*CurrentModule)->Start(NextBlock))
				return;
	}
	bool Start(std::move_only_function<void()> &FC) override {
		Abort();
		for (CurrentModule = std::cbegin(SubPointers); CurrentModule < std::cend(SubPointers); ++CurrentModule) {
			if ((*CurrentModule)->Start(NextBlock)) {
				NextBlock = [this, &FC]() {
					while (++CurrentModule < std::cend(SubPointers))
						if ((*CurrentModule)->Start(NextBlock))
							return;
					FC();
				};
				return true;
			}
		}
		return false;
	}
	static constexpr uint16_t NumTrials = _Sum<_IDModule_t<SubModules>::NumTrials...>::value;
	InfoImplement;
};
template<>
struct _Sequential<> : Module {
protected:
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 0;
	};
#pragma pack(pop)
public:
	_Sequential(Process &Container)
	  : Module(Container) {
	}
	static constexpr uint16_t NumTrials = 0;
	InfoImplement;
};
template<typename... SubModules>
UID const _Sequential<SubModules...>::ID = UID::Module_Sequential;

namespace detail {
template<typename... Ts>
struct type_list {
};

template<typename A, typename B>
struct concat;
template<typename... A, typename... B>
struct concat<type_list<A...>, type_list<B...>> {
	using type = type_list<A..., B...>;
};

template<typename... Args>
struct flatten_pack;

template<typename T>
struct flatten_one {
	using type = type_list<T>;
};

template<typename... Inner>
struct flatten_one<_Sequential<Inner...>> {
	using type = typename flatten_pack<Inner...>::type;
};

template<>
struct flatten_pack<> {
	using type = type_list<>;
};

template<typename Head, typename... Tail>
struct flatten_pack<Head, Tail...> {
	using type = typename concat<
	  typename flatten_one<Head>::type,
	  typename flatten_pack<Tail...>::type>::type;
};

template<typename List>
struct list_to_seq;
template<typename... Ts>
struct list_to_seq<type_list<Ts...>> {
	using type = _Sequential<Ts...>;
};
}
// 依次执行模块。嵌套的Sequential会被展开。
template<typename... Args>
using Sequential = typename detail::list_to_seq<
  typename detail::flatten_pack<Args...>::type>::type;

struct _TimedModule : Module {
	using Module::Module;
	void Abort() override {
		if (Timer) {
			Timer->Stop();
			UnregisterTimer();
		}
	}

protected:
	Timers_one_for_all::TimerClass *Timer = nullptr;
	// 不检查当前Timer是否有效
	void UnregisterTimer() {
		Module::Container.UnregisterTimer(Timer);
		Timer = nullptr;
	}
	struct _UnregisterTimer {
		_TimedModule *const ContentModule;
		void operator()() const {
			ContentModule->UnregisterTimer();
		}
	};
};
struct _Delay : _TimedModule {
	using _TimedModule::_TimedModule;
	void Restart() override {
		if (!Timer)
			Timer = Module::Container.AllocateTimer();
	}
	bool Start(std::move_only_function<void()> &FC) override {
		// 在冷静阶段，Restart会被高频执行，因此只能牺牲一下Start，确保Restart的效率
		FinishCallback = [this, &FC]() {
			UnregisterTimer();
			FC();
		};
		Restart();
		return true;
	}

protected:
	// 确保直接Restart也能正常释放计时器
	std::move_only_function<void()> FinishCallback{ _TimedModule::_UnregisterTimer{ this } };
};
template<typename Unit = Infinite, typename Value = Infinite>
struct Delay : _Delay {
protected:
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 3;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<Delay>::ID };
		PodField<UID> const UnitField{ UID::Field_Unit, _TypeID<Unit>::value };
		PodField<UID const *> const Duration{ UID::Field_Duration, &_ModuleID<Value>::ID };
	};
#pragma pack(pop)
	Value const *const DurationPtr = Module::Container.LoadModule<Value>();

public:
	using _Delay::_Delay;
	void Restart() override {
		_Delay::Restart();  // 不能用_TimedModule，调不到_Delay版本
		_TimedModule::Timer->DoAfter(Unit{ DurationPtr->Current() }, FinishCallback);
	}
	InfoImplement;
};
template<typename Unit, typename Value>
UID const Delay<Unit, Value>::ID = UID::Module_Delay;
template<>
struct Delay<Infinite, Infinite> : Module {
protected:
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 2;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<Delay>::ID };
		UID const Field2Name = UID::Field_Duration;
		UID const Field2Type = UID::Type_Infinite;
	};
#pragma pack(pop)
public:
	using Module::Module;
	bool Start(std::move_only_function<void()> &FC) override {
		return true;
	}
	InfoImplement;
};
template<typename Content, typename Period>
struct _RepeatEvery : _TimedModule {
	using _TimedModule::_TimedModule;
	void Abort() override {
		ContentPtr->Abort();
		_TimedModule::Abort();
	}
	void Restart() override {
		if (Timer)
			ContentPtr->Abort();
		else
			Timer = Module::Container.AllocateTimer();
	}
	bool Start(std::move_only_function<void()> &FC) override {
		// 默认的无限重复，不需要FinishCallback。
		Restart();
		return true;
	}

protected:
	Module *const ContentPtr = Module::Container.LoadModule<Content>();
	std::move_only_function<void()> RepeatCallback{ _EmptyStart{ ContentPtr } };
	Period const *const PeriodPtr = Module::Container.LoadModule<Period>();
};
/*
每隔指定周期，执行一次内容模块。这个周期就是最终的绝对的周期长度，内容模块异步执行，不占用周期时间。第一次执行也需要等待周期时长之后。
此模块为固定周期的高频循环优化，比组合使用Repeat和Delay效率更高更方便。但是，一旦此模块开始，周期长度即固定。如果采用随机周期值，那个周期值将在模块开始时确定，后续再改变随机变量值也不会改变此模块的周期，除非重启。因此，不能使用此模块实现每个周期时长随机。要实现此效果，请组合使用Repeat和Delay模块。
*/
template<typename Content, typename Unit, typename Period, typename Times = Infinite>
struct RepeatEvery : _RepeatEvery<Content, Period> {
protected:
	std::move_only_function<void()> FinishCallback{ _TimedModule::_UnregisterTimer{ this } };
	Times const *const TimesPtr = Module::Container.LoadModule<Times>();
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 5;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<RepeatEvery>::ID };
		PodField<UID const *> const ContentField{ UID::Field_Content, &_ModuleID<Content>::ID };
		PodField<UID> const UnitField{ UID::Field_Unit, _TypeID<Unit>::value };
		PodField<UID const *> const PeriodField{ UID::Field_Period, &_ModuleID<Period>::ID };
		PodField<UID const *> const TimesField{ UID::Field_Times, &_ModuleID<Times>::ID };
	};
#pragma pack(pop)
	using MyBase = _RepeatEvery<Content, Period>;

public:
	using MyBase::_RepeatEvery;
	void Restart() override {
		MyBase::Restart();
		_TimedModule::Timer->RepeatEvery(Unit{ MyBase::PeriodPtr->Current() }, MyBase::RepeatCallback, TimesPtr->Current(), FinishCallback);
	}
	bool Start(std::move_only_function<void()> &FC) override {
		if (!TimesPtr->Current())
			return false;
		FinishCallback = [this, &FC]() {
			this->UnregisterTimer();
			FC();
		};
		Restart();
		return true;
	}
	static constexpr uint16_t NumTrials = _IDModule_t<Content>::NumTrials * Times::NumTrials;
	InfoImplement;
};
template<typename Content, typename Unit, typename Period, typename Times>
UID const RepeatEvery<Content, Unit, Period, Times>::ID = UID::Module_RepeatEvery;
template<typename Content, typename Unit, typename Period>
struct RepeatEvery<Content, Unit, Period, Infinite> : _RepeatEvery<Content, Period> {
protected:
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 5;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<RepeatEvery>::ID };
		PodField<UID const *> const ContentField{ UID::Field_Content, &_ModuleID<Content>::ID };
		PodField<UID> const UnitField{ UID::Field_Unit, _TypeID<Unit>::value };
		PodField<UID const *> const PeriodField{ UID::Field_Period, &_ModuleID<Period>::ID };
		UID const TimesFieldName = UID::Field_Times;
		UID const TimesFieldType = UID::Type_Infinite;
	};
#pragma pack(pop)
	using MyBase = _RepeatEvery<Content, Period>;

public:
	using MyBase::_RepeatEvery;
	void Restart() override {
		MyBase::Restart();
		_TimedModule::Timer->RepeatEvery(Unit{ MyBase::PeriodPtr->Current() }, MyBase::RepeatCallback);
	}
	InfoImplement;
};
template<typename Content, typename Unit, typename Period>
UID const RepeatEvery<Content, Unit, Period, Infinite>::ID = UID::Module_RepeatEvery;
template<typename ContentA, typename ContentB, typename PeriodA, typename PeriodB>
struct _DoubleRepeat : _TimedModule {
	using _TimedModule::_TimedModule;
	void Abort() override {
		ContentAPtr->Abort();
		ContentBPtr->Abort();
		_TimedModule::Abort();
	}
	void Restart() override {
		if (Timer) {
			ContentAPtr->Abort();
			ContentBPtr->Abort();
		} else
			Timer = Module::Container.AllocateTimer();
	}
	bool Start(std::move_only_function<void()> &FC) override {
		// 默认的无限重复，不需要FinishCallback。
		Restart();
		return true;
	}

protected:
	Module *const ContentAPtr = Module::Container.LoadModule<ContentA>();
	Module *const ContentBPtr = Module::Container.LoadModule<ContentB>();
	std::move_only_function<void()> RepeatCallbackA{ _EmptyStart{ ContentAPtr } };
	std::move_only_function<void()> RepeatCallbackB{ _EmptyStart{ ContentBPtr } };
	PeriodA const *const PeriodAPtr = Module::Container.LoadModule<PeriodA>();
	PeriodB const *const PeriodBPtr = Module::Container.LoadModule<PeriodB>();
};
/*
等待时间A后，执行一次模块A；再等待时间B后，执行一次模块B，然后循环。周期总时长就是两个时间之和，不等待模块本身的执行时间。
此模块为固定周期的高频循环优化，比组合使用Repeat和Delay效率更高更方便。但是，一旦此模块开始，周期长度即固定。如果采用随机周期值，那个周期值将在模块开始时确定，后续再改变随机变量值也不会改变此模块的周期，除非重启。因此，不能使用此模块实现每个周期时长随机。要实现此效果，请组合使用Repeat和Delay模块。重复次数为半周期数，即ContentA和ContentB共计执行Times次。
*/
template<typename ContentA, typename ContentB, typename Unit, typename PeriodA, typename PeriodB, typename Times = Infinite>
struct DoubleRepeat : _DoubleRepeat<ContentA, ContentB, PeriodA, PeriodB> {
protected:
	std::move_only_function<void()> FinishCallback{ _TimedModule::_UnregisterTimer{ this } };
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 7;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<DoubleRepeat>::ID };
		PodField<UID const *> const ContentAField{ UID::Field_ContentA, &_ModuleID<ContentA>::ID };
		PodField<UID const *> const ContentBField{ UID::Field_ContentB, &_ModuleID<ContentB>::ID };
		PodField<UID> const UnitField{ UID::Field_Unit, _TypeID<Unit>::value };
		PodField<UID const *> const PeriodAField{ UID::Field_PeriodA, &_ModuleID<PeriodA>::ID };
		PodField<UID const *> const PeriodBField{ UID::Field_PeriodB, &_ModuleID<PeriodB>::ID };
		PodField<UID const *> const TimesField{ UID::Field_Times, &_ModuleID<Times>::ID };
	};
#pragma pack(pop)
	Times const *const TimesPtr = Module::Container.LoadModule<Times>();
	using MyBase = _DoubleRepeat<ContentA, ContentB, PeriodA, PeriodB>;

public:
	using MyBase::_DoubleRepeat;
	void Restart() override {
		MyBase::Restart();
		_TimedModule::Timer->DoubleRepeat(Unit{ MyBase::PeriodAPtr->Current() }, MyBase::RepeatCallbackA, Unit{ MyBase::PeriodBPtr->Current() }, MyBase::RepeatCallbackB, TimesPtr->Current(), FinishCallback);
	}
	bool Start(std::move_only_function<void()> &FC) override {
		if (!TimesPtr->Current())
			return false;
		FinishCallback = [this, &FC]() {
			this->UnregisterTimer();
			FC();
		};
		Restart();
		return true;
	}
	static constexpr uint16_t NumTrials = (Times::NumTrials + 1) / 2 * _IDModule_t<ContentA>::NumTrials + Times::NumTrials / 2 * _IDModule_t<ContentB>::NumTrials;
	InfoImplement;
};
template<typename ContentA, typename ContentB, typename Unit, typename PeriodA, typename PeriodB, typename Times>
UID const DoubleRepeat<ContentA, ContentB, Unit, PeriodA, PeriodB, Times>::ID = UID::Module_DoubleRepeat;
template<typename ContentA, typename ContentB, typename Unit, typename PeriodA, typename PeriodB>
struct DoubleRepeat<ContentA, ContentB, Unit, PeriodA, PeriodB, Infinite> : _DoubleRepeat<ContentA, ContentB, PeriodA, PeriodB> {
protected:
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 7;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<DoubleRepeat>::ID };
		PodField<UID const *> const ContentAField{ UID::Field_ContentA, &_ModuleID<ContentA>::ID };
		PodField<UID const *> const ContentBField{ UID::Field_ContentB, &_ModuleID<ContentB>::ID };
		PodField<UID> const UnitField{ UID::Field_Unit, _TypeID<Unit>::value };
		PodField<UID const *> const PeriodAField{ UID::Field_PeriodA, &_ModuleID<PeriodA>::ID };
		PodField<UID const *> const PeriodBField{ UID::Field_PeriodB, &_ModuleID<PeriodB>::ID };
		UID const TimesFieldName = UID::Field_Times;
		UID const TimesFieldType = UID::Type_Infinite;
	};
#pragma pack(pop)
	using MyBase = _DoubleRepeat<ContentA, ContentB, PeriodA, PeriodB>;

public:
	using MyBase::_DoubleRepeat;
	void Restart() override {
		MyBase::Restart();
		_TimedModule::Timer->DoubleRepeat(Unit{ MyBase::PeriodAPtr->Current() }, MyBase::RepeatCallbackA, Unit{ MyBase::PeriodBPtr->Current() }, MyBase::RepeatCallbackB);
	}
	InfoImplement;
};
template<typename ContentA, typename ContentB, typename Unit, typename PeriodA, typename PeriodB>
UID const DoubleRepeat<ContentA, ContentB, Unit, PeriodA, PeriodB, Infinite>::ID = UID::Module_DoubleRepeat;
struct _InstantaneousModule : Module {
	using Module::Module;
	bool Start(std::move_only_function<void()> &FC) override {
		Restart();
		return false;
	}
};
template<typename Target>
struct ModuleAbort : _InstantaneousModule {
protected:
	Module *const TargetPtr = Module::Container.LoadModule<Target>();
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 2;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<ModuleAbort>::ID };
		PodField<UID const *> const TargetField{ UID::Field_Target, &_ModuleID<Target>::ID };
	};
#pragma pack(pop)
public:
	using _InstantaneousModule::_InstantaneousModule;
	void Restart() override {
		TargetPtr->Abort();
	}
	InfoImplement;
};
template<typename Target>
UID const ModuleAbort<Target>::ID = UID::Module_Abort;
template<typename Target>
struct ModuleRestart : _InstantaneousModule {
protected:
	Module *const TargetPtr = Module::Container.LoadModule<Target>();
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 2;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<ModuleRestart>::ID };
		PodField<UID const *> const TargetField{ UID::Field_Target, &_ModuleID<Target>::ID };
	};
#pragma pack(pop)
public:
	using _InstantaneousModule::_InstantaneousModule;
	void Restart() override {
		TargetPtr->Restart();
	}
	InfoImplement;
};
template<typename Target>
UID const ModuleRestart<Target>::ID = UID::Module_Restart;
template<typename Target>
struct ModuleRandomize : _InstantaneousModule {
protected:
	IRandom *const TargetPtr = Module::Container.LoadModule<Target>();
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 2;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<ModuleRandomize>::ID };
		PodField<UID const *> const TargetField{ UID::Field_Target, &_ModuleID<Target>::ID };
	};
#pragma pack(pop)
public:
	using _InstantaneousModule::_InstantaneousModule;
	void Restart() override {
		TargetPtr->Randomize();
	}
	InfoImplement;
};
template<typename Target>
UID const ModuleRandomize<Target>::ID = UID::Module_Randomize;
template<uint8_t Pin, bool HighOrLow>
struct DigitalWrite : _InstantaneousModule {
protected:
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 3;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<DigitalWrite>::ID };
		PodField<uint8_t> const PinField{ UID::Field_Pin, Pin };
		PodField<bool> const HighOrLowField{ UID::Field_HighOrLow, HighOrLow };
	};
#pragma pack(pop)
public:
	DigitalWrite(Process &Container)
	  : _InstantaneousModule(Container) {
		Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
	}
	void Restart() override {
		Quick_digital_IO_interrupt::DigitalWrite<Pin, HighOrLow>();
	}
	InfoImplement;
};
template<uint8_t Pin, bool HighOrLow>
UID const DigitalWrite<Pin, HighOrLow>::ID = UID::Module_DigitalWrite;
template<uint8_t Pin>
struct DigitalToggle : _InstantaneousModule {
protected:
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 2;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<DigitalToggle>::ID };
		PodField<uint8_t> const PinField{ UID::Field_Pin, Pin };
	};
#pragma pack(pop)
public:
	DigitalToggle(Process &Container)
	  : _InstantaneousModule(Container) {
		Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
	}
	void Restart() override {
		Quick_digital_IO_interrupt::DigitalToggle<Pin>();
	}
	InfoImplement;
};
template<uint8_t Pin>
UID const DigitalToggle<Pin>::ID = UID::Module_DigitalToggle;
// 此模块可以用ModuleAbort停止监视
template<uint8_t Pin, typename Monitor>
struct MonitorPin : _InstantaneousModule {
protected:
	Module *const MonitorPtr = Module::Container.LoadModule<Monitor>();
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 3;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<MonitorPin>::ID };
		PodField<uint8_t> const PinField{ UID::Field_Pin, Pin };
		PodField<UID const *> const MonitorField{ UID::Field_Monitor, &_ModuleID<Monitor>::ID };
	};
#pragma pack(pop)
public:
	MonitorPin(Process &Container)
	  : _InstantaneousModule(Container) {
		Quick_digital_IO_interrupt::PinMode<Pin, INPUT>();
	}
	PinListener *Listener = nullptr;
	void Abort() override {
		if (Listener) {
			Module::Container.UnregisterInterrupt(Listener);
			Listener = nullptr;
		}
	}
	void Restart() override {
		Abort();
		Listener = Module::Container.RegisterInterrupt(Pin, [MonitorPtr = MonitorPtr]() {
			MonitorPtr->Start(_EmptyCallback);
		});
	}
	InfoImplement;
};
template<uint8_t Pin, typename Monitor>
UID const MonitorPin<Pin, Monitor>::ID = UID::Module_MonitorPin;
template<UID Message>
struct SerialMessage : _InstantaneousModule {
protected:
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 2;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<SerialMessage>::ID };
		PodField<UID> const MessageField{ UID::Field_Message, Message };
	};
#pragma pack(pop)
public:
	using _InstantaneousModule::_InstantaneousModule;
	void Restart() override {
		SerialStream.AsyncInvoke(static_cast<Async_stream_IO::Port>(UID::PortC_Signal), &Container, Message);
	}
	InfoImplement;
};
template<UID Message>
UID const SerialMessage<Message>::ID = UID::Module_SerialMessage;
// 被Async包装的延时模块将异步执行，即不等待其结束直接返回继续。但是，对其调用Abort仍可以放弃内容模块。
template<typename Content>
struct Async : _InstantaneousModule {
protected:
	Module *const ContentPtr = Module::Container.LoadModule<Content>();
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 2;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<Async>::ID };
		PodField<UID const *> const ContentField{ UID::Field_Content, &_ModuleID<Content>::ID };
	};
#pragma pack(pop)
public:
	using _InstantaneousModule::_InstantaneousModule;
	void Abort() override {
		ContentPtr->Abort();
	}
	void Restart() override {
		Abort();
		ContentPtr->Start(_EmptyCallback);
	}
	static constexpr uint16_t NumTrials = _IDModule_t<Content>::NumTrials;
	InfoImplement;
};
template<typename Content>
UID const Async<Content>::ID = UID::Module_Async;
// 回合是断线重连后自动恢复的最小单元，并且在回合开始时向串口发送信号
template<UID TrialID, typename Content>
struct Trial : Module {
protected:
	Module *const ContentPtr = Module::Container.LoadModule<Content>();
	std::move_only_function<void()> *FinishCallback = &_EmptyCallback;
	void _Restart() {
		Abort();
		SerialStream.AsyncInvoke(static_cast<uint8_t>(UID::PortC_TrialStart), &Container, TrialID);
	}
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 2;
		PodField<UID> const ID{ UID::Field_ID, TrialID };
		PodField<UID const *> const ContentField{ UID::Field_Content, &_ModuleID<Content>::ID };
	};
#pragma pack(pop)
public:
	using Module::Module;
	void Abort() override {
		ContentPtr->Abort();
	}
	void Restart() override {
		_Restart();
		ContentPtr->Start(*FinishCallback);
	}
	bool Start(std::move_only_function<void()> &FC) override {
		auto const It = Module::Container.TrialsDone.find(TrialID);
		if (It != Module::Container.TrialsDone.end())
			if (It->second) {
				if (!--It->second)
					Module::Container.TrialsDone.erase(It);
				return false;
			} else
				Module::Container.TrialsDone.erase(It);
		_Restart();
		if (ContentPtr->Start(FC)) {
			FinishCallback = &FC;
			return true;
		}
		return false;
	}
	static constexpr uint16_t NumTrials = 1;
	InfoImplement;
};
template<UID TrialID, typename Content>
UID const Trial<TrialID, Content>::ID = TrialID;
// 将一个清理模块附加到目标模块上，开始、重启、终止或析构此模块前都将先执行清理模块，但目标模块正常结束时则不会清理。清理模块一般应是瞬时的，如果有延时操作则不会等待其完成。
template<typename Target, typename Cleaner>
struct CleanWhenAbort : Module {
protected:
	Module *const TargetPtr = Module::Container.LoadModule<Target>();
	std::move_only_function<void()> *FinishCallback = &_EmptyCallback;
	std::move_only_function<void()> TargetCallback = [this]() {
		Module::Container.ExtraCleaners.erase(&StartCleaner);
		(*FinishCallback)();
	};
	std::move_only_function<void()> StartCleaner = [CleanerPtr = Module::Container.LoadModule<Cleaner>()]() {
		CleanerPtr->Start(_EmptyCallback);
	};
	void _Abort() {
		TargetPtr->Abort();
		StartCleaner();
	}
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 3;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<CleanWhenAbort>::ID };
		PodField<UID const *> const TargetField{ UID::Field_Target, &_ModuleID<Target>::ID };
		PodField<UID const *> const CleanerField{ UID::Field_Cleaner, &_ModuleID<Cleaner>::ID };
	};
#pragma pack(pop)
public:
	using Module::Module;
	void Abort() override {
		_Abort();
		Module::Container.ExtraCleaners.erase(&StartCleaner);
	}
	void Restart() override {
		_Abort();
		if (TargetPtr->Start(TargetCallback))
			Module::Container.ExtraCleaners.insert(&StartCleaner);
	}
	bool Start(std::move_only_function<void()> &FC) override {
		_Abort();
		if (TargetPtr->Start(TargetCallback)) {
			FinishCallback = &FC;
			Module::Container.ExtraCleaners.insert(&StartCleaner);
			return true;
		}
		return false;
	}
	~CleanWhenAbort() {
		StartCleaner();
		Module::Container.ExtraCleaners.erase(&StartCleaner);
	}
	static constexpr uint16_t NumTrials = _IDModule_t<Target>::NumTrials;
	InfoImplement;
};
template<typename Target, typename Cleaner>
UID const CleanWhenAbort<Target, Cleaner>::ID = UID::Module_CleanWhenAbort;
template<UID UniqueID = UID::Module_DynamicSlot>
struct DynamicSlot : Module {
protected:
#pragma pack(push, 1)
	struct InfoStruct {
		uint8_t const NumFields = 1;
		PodField<UID> const ID{ UID::Field_ID, _ModuleID<DynamicSlot>::ID };
	};
#pragma pack(pop)
public:
	using Module::Module;
	void Abort() override {
		if (ContentPtr)
			ContentPtr->Abort();
	}
	void Restart() override {
		if (ContentPtr)
			ContentPtr->Restart();
	}
	bool Start(std::move_only_function<void()> &FC) override {
		return ContentPtr && ContentPtr->Start(FC);
	}
	Module *ContentPtr = nullptr;
	InfoImplement;
	template<typename Content>
	struct Load : _InstantaneousModule {
	protected:
		DynamicSlot *const SlotPtr = Module::Container.LoadModule<DynamicSlot>();
		Module *const ContentPtr;
#pragma pack(push, 1)
		struct InfoStruct {
			uint8_t const NumFields = 3;
			PodField<UID> const ID{ UID::Field_ID, _ModuleID<Load>::ID };
			PodField<UID const *> const Slot{ UID::Field_Slot, &_ModuleID<DynamicSlot>::ID };
			PodField<UID const *> ContentField;
			constexpr InfoStruct()
			  : ContentField{ UID::Field_Content, &_ModuleID<Content>::ID } {}
		};
#pragma pack(pop)
	public:
		//SAM编译器bug：使用了内联struct模板参数的成员初始化只能写在构造方法里，不能直接成员初始化，成员初始化无法访问内联模板参数
		Load(Process &Container)
		  : _InstantaneousModule(Container), ContentPtr{ Container.LoadModule<Content>() } {}
		void Restart() override {
			SlotPtr->ContentPtr = ContentPtr;
		}
		InfoImplement;
	};
	struct Clear : _InstantaneousModule {
	protected:
		DynamicSlot *const SlotPtr = Module::Container.LoadModule<DynamicSlot>();
#pragma pack(push, 1)
		struct InfoStruct {
			uint8_t const NumFields = 2;
			PodField<UID> const ID{ UID::Field_ID, _ModuleID<Clear>::ID };
			PodField<UID const *> const Slot{ UID::Field_Slot, &_ModuleID<DynamicSlot>::ID };
		};
#pragma pack(pop)
	public:
		using _InstantaneousModule::_InstantaneousModule;
		void Restart() override {
			SlotPtr->ContentPtr = nullptr;
		}
		InfoImplement;
	};
};
template<UID UniqueID>
UID const DynamicSlot<UniqueID>::ID = UniqueID;
template<UID UniqueID>
template<typename Content>
UID const DynamicSlot<UniqueID>::Load<Content>::ID = UID::Module_LoadSlot;
template<UID UniqueID>
UID const DynamicSlot<UniqueID>::Clear::ID = UID::Module_ClearSlot;

template<typename TModule>
uint16_t Session(Process *P) {
	return P->LoadStartModule<TModule>();
};
#define Pin static constexpr uint8_t
template<typename Target>
UID const &_ModuleID<Target>::ID = Target::ID;

// 将模块绑定到指定UID，方便循环引用，并在返回信息中显示自定义名称
#define AssignModuleID(TargetModule, TargetID) \
	template<> \
	struct _ModuleID<TargetModule> { \
		static UID const ID; \
	}; \
	UID const _ModuleID<TargetModule>::ID = TargetID; \
	template<> \
	struct _IDModule<IDModule<TargetID>> { \
		using type = TargetModule; \
	}; \
	template<> \
	struct _ModuleID<IDModule<TargetID>> : _ModuleID<TargetModule> { \
	};