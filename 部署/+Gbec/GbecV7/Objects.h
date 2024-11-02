#pragma once
#include "UID.h"
#include "Async_stream_IO.hpp"
#include <unordered_map>
#include <random>
#include <dynarray>
#include <span>
#include <utility>
#include <numeric>

// 进度与异常回调

#pragma pack(push, 1)
struct StackFrame
{
	uint8_t StackLevel;
	UID Template;
	uint16_t Progress;
};
struct ExceptionProgress
{
	UID Type = UID::Progress_Exception;
	UID Exception;
	constexpr ExceptionProgress(UID Exception) : Exception(Exception) {}
};
template <typename T>
struct CustomProgress
{
	UID Type;
	uint8_t StackLevel;
	T Progress;
	constexpr CustomProgress(uint8_t StackLevel, T Progress, UID Type = UID::Progress_Overwrite) : Type(Type), StackLevel(StackLevel), Progress(Progress) {}
};
template <typename T>
struct SignalProgress
{
	UID Type = UID::Progress_Signal;
	T Signal;
	constexpr SignalProgress(T Signal) : Signal(Signal) {}
};
#pragma pack(pop)
struct CallbackMessage
{
	UID Exception;
	std::vector<StackFrame> StackTrace;
};

// 对象基础

// 所有派生类应当支持无参构造，以便在ObjectCreators中注册。
struct Object
{
	/*开始Object，指定用于接收进度信息的远程端口和堆栈层级。不支持进度的Object可以忽略这两个参数。Trial是引用类型，因此被调用的Object可以借此向调用方返回进度信息。
	如果任务在本次调用的上下文中即完成，返回Exception_ObjectFinished，并且不应再调用FinishCallback。如果任务依赖某个延迟发生的条件才算完成，本次调用应返回Exception_StillRunning，并在任务彻底完成时调用FinishCallback。
	调用方应根据返回值决定是否要等待FinishCallback被调用。如果返回Exception_ObjectFinished，应立即调度后续任务，可以认为FinishCallback不会被调用。如果返回Exception_StillRunning，应在FinishCallback中调度后续任务。
	FinishCallback通常会被中断调用，因此可能会与其访问相同资源的过程通常应当禁用中断。
	如果
	*/
	virtual UID Start() { return UID::Exception_MethodNotSupported; }
	virtual UID Restore(std::span<const uint8_t> ProgressInfo) { return UID::Exception_MethodNotSupported; }
	// 支持重复的Object应override此方法，重复执行Times次，返回Exception_Success。
	virtual UID Repeat(uint16_t Times) { return UID::Exception_MethodNotSupported; }
	// 支持暂停的Object应override此方法，暂停当前执行，返回Exception_Success。调用方应检查返回值判断当前Object是否支持暂停，在不支持暂停的情况下采取其它措施实现暂停。
	virtual UID Pause() { return UID::Exception_MethodNotSupported; }
	// 支持暂停的Object应override此方法，继续上次暂停的执行。
	virtual UID Continue() { return UID::Exception_MethodNotSupported; }
	// 支持废弃的Object应override此方法，废弃当前执行，返回Exception_Success。调用方应检查返回值判断当前Object是否支持废弃，在不支持废弃的情况下采取其它措施实现废弃。
	virtual UID Abort() { return UID::Exception_MethodNotSupported; }
	// 支持获取信息的Object应override此方法
	virtual UID GetInformation(std::dynarray<char> &Container) { return UID::Exception_MethodNotSupported; }
	virtual ~Object() {}
	virtual void FinishCallback(std::unique_ptr<CallbackMessage> Message) {}

protected:
	const uint8_t ProgressPort;
	Object(uint8_t ProgressPort) : ProgressPort(ProgressPort) {}
};
struct RootObject : Object
{
	Object *Child;
	RootObject(uint8_t ProgressPort) : Object(ProgressPort) {}
	virtual ~RootObject() { delete Child; }
	UID Start() override
	{
		return Child->Start();
	};
	/*ProgressInfo线性紧密排列每个栈位的自定义进度信息：
	{
		uint8_t NumBytes;//本栈位的自定义进度信息字节数
		char Data[NumBytes];//本栈位的自定义进度信息
	}
	每个栈位应当负责将ProgressInfo=ProgressInfo.subspan(NumBytes+1)再传递给下一个栈位。
	*/
	UID Restore(std::span<const uint8_t> ProgressInfo) override
	{
		return Child->Restore(ProgressInfo);
	}
	UID Repeat(uint16_t Times) override
	{
		return Child->Repeat(Times);
	}
	UID Pause() override
	{
		return Child->Pause();
	}
	UID Continue() override
	{
		return Child->Continue();
	}
	UID Abort() override
	{
		return Child->Abort();
	}
	UID GetInformation(std::dynarray<char> &Container) override
	{
		return Child->GetInformation(Container);
	}

protected:
	void FinishCallback(std::unique_ptr<CallbackMessage> Message) override
	{
		Async_stream_IO::Send([Message = std::move(Message)](const std::move_only_function<void(const void *Message, uint8_t Size) const> &MessageSender)
							  {
				ExceptionProgress Header(Message->Exception);
				if (Header.Exception == UID::Exception_Success)
					MessageSender(&Header, sizeof(ExceptionProgress));
				else
				{
					const uint8_t Size = sizeof(ExceptionProgress) + sizeof(StackFrame) * Message->StackTrace.size();
					std::unique_ptr<char[]> Buffer = std::make_unique_for_overwrite<char[]>(Size);
					*reinterpret_cast<ExceptionProgress*>(Buffer.get()) = Header;
					std::copy(Message->StackTrace.cbegin(), Message->StackTrace.cend(), reinterpret_cast<StackFrame*>(reinterpret_cast<ExceptionProgress*>(Buffer.get()) + 1));
					MessageSender(Buffer.get(), Size);
				} }, ProgressPort);
	}
};
struct ChildObject : Object
{
	Object *const Parent;
	const uint8_t StackLevel;
	UID ClassID;
	ChildObject(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel, UID ClassID) : Object(ProgressPort), Parent(Parent), StackLevel(StackLevel), ClassID(ClassID) {}
};
template <typename ObjectType>
inline RootObject *New(uint8_t ProgressPort)
{
	RootObject *const Root = new RootObject(ProgressPort);
	Root->Child = new ObjectType(ProgressPort, Root, 0);
	return Root;
}

// 信息架构

#define OverrideGetInformation                                         \
	UID GetInformation(std::dynarray<char> &Container) override        \
	{                                                                  \
		Container = std::dynarray<char>(sizeof(Information));          \
		memcpy_P(Container.data(), &Information, sizeof(Information)); \
		return UID::Exception_Success;                                 \
	}
template <typename T>
struct TypeID;
template <>
struct TypeID<bool>
{
	static constexpr UID value = UID::Type_Bool;
};
template <>
struct TypeID<uint8_t>
{
	static constexpr UID value = UID::Type_UInt8;
};
template <>
struct TypeID<uint16_t>
{
	static constexpr UID value = UID::Type_UInt16;
};
template <>
struct TypeID<UID>
{
	static constexpr UID value = UID::Type_UID;
};
#pragma pack(push, 1)
template <typename T, typename... Ts>
struct InfoArray
{
	static constexpr UID ID = UID::Type_Array;
	uint8_t Number = sizeof...(Ts) + 1;
	UID Type = TypeID<typename T::value_type>::value;
	typename T::value_type Array[sizeof...(Ts) + 1] = {T::value, Ts::value...};
};
template <typename T, typename... Ts>
struct CellArray
{
	UID Type = TypeID<T>::value;
	T Value;
	CellArray<Ts...> Values;
	constexpr CellArray(T Value, Ts... Values)
		: Value(Value), Values(CellArray<Ts...>(Values...)) {}
};
template <typename T>
struct CellArray<T>
{
	UID Type = TypeID<T>::value;
	T Value;
	constexpr CellArray(T Value)
		: Value(Value) {}
};
template <typename... T>
struct InfoCell
{
	static constexpr UID ID = UID::Type_Cell;
	uint8_t NumCells = 0;
};
template <typename T, typename... Ts>
struct InfoCell<T, Ts...>
{
	static constexpr UID ID = UID::Type_Cell;
	uint8_t NumCells = sizeof...(Ts) + 1;
	CellArray<T, Ts...> Values;
	constexpr InfoCell(T Value, Ts... Values)
		: Values(CellArray<T, Ts...>(Value, Values...)) {}
};
// 只有主模板能推断，特化模板必须加推断向导
template <typename T, typename... Ts>
InfoCell(T, Ts...) -> InfoCell<T, Ts...>;
template <typename TName, typename TValue, typename... Ts>
struct InfoFields
{
	TName Name;
	UID Type = TypeID<TValue>::value;
	TValue Value;
	InfoFields<Ts...> FollowingFields;
	constexpr InfoFields(TName Name, TValue Value, Ts... NameValues)
		: Name(Name), Value(Value), FollowingFields(InfoFields<Ts...>(NameValues...)) {}
};
template <typename TName, typename TValue>
struct InfoFields<TName, TValue>
{
	TName Name;
	UID Type = TypeID<TValue>::value;
	TValue Value;
	constexpr InfoFields(TName Name, TValue Value)
		: Name(Name), Value(Value) {}
};
template <typename... Ts>
struct InfoStruct
{
	constexpr static UID ID = UID::Type_Struct;
	uint8_t NumFields = 0;
	constexpr InfoStruct(Ts... NameValues) {}
};
template <typename T, typename... Ts>
struct InfoStruct<UID, T, Ts...>
{
	constexpr static UID ID = UID::Type_Struct;
	uint8_t NumFields = sizeof...(Ts) / 2 + 1;
	InfoFields<UID, T, Ts...> Fields;
	constexpr InfoStruct(UID Name, T Value, Ts... NameValues)
		: Fields(InfoFields<UID, T, Ts...>(Name, Value, NameValues...)) {}
};
#ifdef __cpp_deduction_guides
// 只有主模板能推断，特化模板必须加推断向导（或者直接限定模板参数不能为空）
template <typename... Ts>
InfoStruct(UID, Ts...) -> InfoStruct<UID, Ts...>;
#endif
#pragma pack(pop)

// 编译期计算

template <uint8_t... V>
struct SumHelper;
template <uint8_t First, uint8_t... Rest>
struct SumHelper<First, Rest...>
{
	static constexpr uint8_t value = First + SumHelper<Rest...>::value;
};
template <>
struct SumHelper<>
{
	static constexpr uint8_t value = 0;
};
template <uint8_t... V>
constexpr uint8_t Sum()
{
	return SumHelper<V...>::value;
}
#define ParameterPackExpand(...) int __CONCAT(_, __LINE__)[] = {(__VA_ARGS__, 0)...};

template <typename>
struct ExpandSequence;
template <size_t... Index>
struct ExpandSequence<std::index_sequence<Index...>>
{
	template <uint16_t... Repeat>
	struct ExpandRepeat
	{
		template <typename TupleType>
		static void TupleFillArray(const TupleType &Tuple, const ChildObject **Array)
		{
			ParameterPackExpand(Array = std::fill_n(Array, Repeat, &std::get<Index>(Tuple)));
		}
	};
	template <typename TupleType>
	static void TupleFillArray(const TupleType &Tuple, const ChildObject **Array)
	{
		ParameterPackExpand(Array[Index] = &std::get<Index>(Tuple));
	}
	template <typename TupleType>
	static void TupleFillArray(const TupleType &Tuple, const ChildObject **Array, const uint16_t *Repeat)
	{
		ParameterPackExpand(Array = std::fill_n(Array, Repeat[Index], &std::get<Index>(Tuple)));
	}
};
template <uint8_t... Index>
struct ExpandSequence<std::integer_sequence<uint8_t, Index...>>
{
	template <uint16_t... Times>
	static void TimesFillIndex(uint8_t *IndexArray)
	{
		ParameterPackExpand(IndexArray = std::fill_n(IndexArray, Times, Index));
	}
	template <uint16_t... Times>
	static void LeftFillIndex(uint8_t *IndexArray, const uint16_t *Done)
	{
		ParameterPackExpand(IndexArray = std::fill_n(IndexArray, Times - Done[Index], Index));
	}
};

// 常用对象模板

// 顺序依次执行所有的Object
template <typename... ObjectType>
struct _Sequential : ChildObject
{
	_Sequential(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel, UID ClassID = UID::TemplateID_Sequential) : ChildObject(ProgressPort, Parent, StackLevel, ClassID), SubObjects(ObjectType(ProgressPort, this, StackLevel + 1)...)
	{
		ExpandSequence<std::index_sequence_for<ObjectType...>>::TupleFillArray(SubObjects, SubPointers);
	}
	UID Start() override
	{
		if (Running)
			return UID::Exception_ObjectNotIdle;
		Progress = 0;
		const UID Result = Iterate();
		Running = Result == UID::Exception_StillRunning;
		return Result;
	}
	UID Restore(std::span<const uint8_t> ProgressInfo) override
	{
		if (Running)
			return UID::Exception_ObjectNotIdle;
		Progress = *(ProgressInfo.data() + 1);
		ProgressInfo = ProgressInfo.subspan(2);
		const UID Result = ProgressInfo.empty() ? Iterate() : SubPointers[Progress]->Restore(ProgressInfo);
		Running = Result == UID::Exception_StillRunning;
		return Result;
	}
	UID Pause() override
	{
		return Running ? SubPointers[Progress]->Pause() : UID::Exception_ObjectNotRunning;
	}
	UID Continue() override
	{
		return Running ? SubPointers[Progress]->Continue() : UID::Exception_ObjectNotPaused;
	}
	UID Abort() override
	{
		if (Running)
		{
			Running = false;
			return SubPointers[Progress]->Abort();
		}
		else
			return UID::Exception_ObjectNotRunning;
	}
	OverrideGetInformation;

protected:
	uint8_t Progress;
	std::tuple<ObjectType...> SubObjects;
	ChildObject *SubPointers[sizeof...(ObjectType)];
	static constexpr auto Information PROGMEM = InfoStruct(UID::Property_TemplateID, UID::TemplateID_Sequential, UID::Property_Subobjects, InfoCell(ObjectType::Information...));
	bool Running = false;
	void FinishCallback(std::unique_ptr<CallbackMessage> ChildResult) override
	{
		if (ChildResult->Exception == UID::Exception_Success)
		{
			Progress++;
			switch (const UID Result = Iterate())
			{
			case UID::Exception_StillRunning:
				if (StackLevel < UINT8_MAX)
					Async_stream_IO::Send(CustomProgress(StackLevel, Progress), ProgressPort);
				break;
			case UID::Exception_Success:
				Running = false;
				Parent->FinishCallback(std::move(ChildResult));
				break;
			default:
				Running = false;
				ChildResult->Exception = Result;
				ChildResult->StackTrace = {{StackLevel, ClassID, Progress}};
				Parent->FinishCallback(std::move(ChildResult));
			}
		}
		else
		{
			Running = false;
			ChildResult->StackTrace.push_back({StackLevel, ClassID, Progress});
			Parent->FinishCallback(std::move(ChildResult));
		}
	}
	UID Iterate()
	{
		while (Progress < sizeof...(ObjectType))
			switch (const UID Result = SubPointers[Progress]->Start();)
			{
			default:
				return Result;
			case UID::Exception_StillRunning:
				return UID::Exception_StillRunning;
			case UID::Exception_Success:
				Progress++;
			}
		return UID::Exception_Success;
	}
	template <uint16_t... Times>
	struct WithRepeat : ChildObject
	{
		WithRepeat(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel, UID ClassID = UID::TemplateID_SequentialRepeat) : ChildObject(ProgressPort, Parent, StackLevel, ClassID), SubObjects(ObjectType(ProgressPort, this, StackLevel + 1)...)
		{
			ExpandSequence<std::index_sequence_for<ObjectType...>>::ExpandRepeat<Times...>::TupleFillArray(SubObjects, SubPointers);
		}
		UID Start() override
		{
			if (Running)
				return UID::Exception_ObjectNotIdle;
			Progress = 0;
			const UID Result = Iterate();
			Running = Result == UID::Exception_StillRunning;
			return Result;
		}
		UID Restore(std::span<const uint8_t> ProgressInfo) override
		{
			if (Running)
				return UID::Exception_ObjectNotIdle;
			Progress = *reinterpret_cast<const uint16_t *>(ProgressInfo.data() + 1);
			ProgressInfo = ProgressInfo.subspan(sizeof(Progress) + 1);
			const UID Result = ProgressInfo.empty() ? Iterate() : SubPointers[Progress]->Restore(ProgressInfo);
			Running = Result == UID::Exception_StillRunning;
			return Result;
		}
		UID Pause() override
		{
			return Running ? SubPointers[Progress]->Pause() : UID::Exception_ObjectNotRunning;
		}
		UID Continue() override
		{
			return Running ? SubPointers[Progress]->Continue() : UID::Exception_ObjectNotPaused;
		}
		UID Abort() override
		{
			if (Running)
			{
				Running = false;
				return SubPointers[Progress]->Abort();
			}
			else
				return UID::Exception_ObjectNotRunning;
		}
		OverrideGetInformation;

	protected:
		uint16_t Progress;
		std::tuple<ObjectType...> SubObjects;
		ChildObject *SubPointers[Sum<Times...>()];
		static constexpr auto Information PROGMEM = InfoStruct(UID::Property_TemplateID, UID::TemplateID_SequentialRepeat, UID::Property_Subobjects, InfoCell(InfoStruct(UID::Property_ObjectInfo, ObjectType::Information, UID::Property_RepeatTime, Times)...));
		bool Running = false;
		void FinishCallback(std::unique_ptr<CallbackMessage> ChildResult) override
		{
			if (ChildResult->Exception == UID::Exception_Success)
			{
				Progress++;
				switch (const UID Result = Iterate())
				{
				case UID::Exception_StillRunning:
					if (StackLevel < UINT8_MAX)
						Async_stream_IO::Send(CustomProgress(StackLevel, Progress), ProgressPort);
					break;
				case UID::Exception_Success:
					Running = false;
					Parent->FinishCallback(std::move(ChildResult));
					break;
				default:
					Running = false;
					ChildResult->Exception = Result;
					ChildResult->StackTrace = {{StackLevel, UID::TemplateID_SequentialRepeat, Progress}};
					Parent->FinishCallback(std::move(ChildResult));
				}
			}
			else
			{
				Running = false;
				ChildResult->StackTrace.push_back({StackLevel, UID::TemplateID_SequentialRepeat, Progress});
				Parent->FinishCallback(std::move(ChildResult));
			}
		}
		UID Iterate()
		{
			while (Progress < Sum<Times...>())
				switch (const UID Result = SubPointers[Progress]->Start();)
				{
				default:
					return Result;
				case UID::Exception_StillRunning:
					return UID::Exception_StillRunning;
				case UID::Exception_Success:
					Progress++;
				}
			return UID::Exception_Success;
		}
	};
};
// 依次执行多个对象
template <typename... ObjectType>
struct Sequential : _Sequential<ObjectType...>
{
	using _Sequential<ObjectType...>::_Sequential;

	// 为对象附加自定义ID
	template <UID CustomID>
	struct WithID : _Sequential<ObjectType...>
	{
		WithID(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel) : _Sequential<ObjectType...>(ProgressPort, Parent, StackLevel, CustomID) {}
		OverrideGetInformation;

	protected:
		static constexpr auto Information PROGMEM = InfoStruct(UID::Property_TemplateID, CustomID, UID::Property_Subobjects, InfoCell(ObjectType::Information...));
	};

	// 每个对象重复执行特定次数
	template <uint16_t... Times>
	struct WithRepeat : _Sequential<ObjectType...>::template WithRepeat<Times...>
	{
		using _Sequential<ObjectType...>::template WithRepeat<Times...>::WithRepeat;

		// 为对象附加自定义ID
		template <UID CustomID>
		struct WithID : _Sequential<ObjectType...>::template WithRepeat<Times...>
		{
			WithID(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel) : _Sequential<ObjectType...>::template WithRepeat<Times...>(ProgressPort, Parent, StackLevel, CustomID) {}
			OverrideGetInformation;

		protected:
			static constexpr auto Information PROGMEM = InfoStruct(UID::Property_TemplateID, CustomID, UID::Property_Subobjects, InfoCell(InfoStruct(UID::Property_ObjectInfo, ObjectType::Information, UID::Property_RepeatTime, Times)...));
		};
	};
};

#ifdef ARDUINO_ARCH_AVR
using ArchUrng = std::ArduinoUrng;
#else
using ArchUrng = std::TrueUrng;
#endif
extern ArchUrng Urng;
template <typename... ObjectType>
struct _Random : ChildObject
{
	_Random(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel, UID ClassID = UID::TemplateID_Random) : ChildObject(ProgressPort, Parent, StackLevel, ClassID), SubObjects(ObjectType(ProgressPort, this, StackLevel + 1)...)
	{
		ExpandSequence<std::index_sequence_for<ObjectType...>>::TupleFillArray(SubObjects, SubPointers);
	}
	UID Start() override
	{
		if (Running)
			return UID::Exception_ObjectNotIdle;
		Progress = 0;
		std::shuffle(std::begin(SubPointers), std::end(SubPointers), Urng);
		const UID Result = Iterate();
		Running = Result == UID::Exception_StillRunning;
		return Result;
	}
	UID Restore(std::span<const uint8_t> ProgressInfo) override
	{
		if (Running)
			return UID::Exception_ObjectNotIdle;
		Progress = *ProgressInfo.data();
		const UID *ObjectsDone = reinterpret_cast<const UID *>(ProgressInfo.data() + 1);
		for (uint8_t P1 = 0; P1 < Progress; ++P1)
		{
			uint8_t P2;
			for (P2 = P1; P2 < sizeof...(ObjectType); ++P2)
				if (ObjectsDone[P1] == SubPointers[P2]->ClassID)
				{
					std::swap(SubPointers[P1], SubPointers[P2]);
					break;
				}
			if (P2 == sizeof...(ObjectType))
				return UID::Exception_ProgressObjectNotFound;
		}
		ProgressInfo = ProgressInfo.subspan(Progress + 1);
		std::shuffle(std::begin(SubPointers) + Progress--, std::end(SubPointers), Urng);
		const UID Result = ProgressInfo.empty() ? Iterate() : SubPointers[Progress]->Restore(ProgressInfo);
		Running = Result == UID::Exception_StillRunning;
		return Result;
	}
	UID Pause() override
	{
		return Running ? SubPointers[Progress]->Pause() : UID::Exception_ObjectNotRunning;
	}
	UID Continue() override
	{
		return Running ? SubPointers[Progress]->Continue() : UID::Exception_ObjectNotPaused;
	}
	UID Abort() override
	{
		if (Running)
		{
			Running = false;
			return SubPointers[Progress]->Abort();
		}
		else
			return UID::Exception_ObjectNotRunning;
	}
	OverrideGetInformation;

protected:
	uint8_t Progress;
	std::tuple<ObjectType...> SubObjects;
	ChildObject *SubPointers[sizeof...(ObjectType)];
	static constexpr auto Information PROGMEM = InfoStruct(UID::Property_TemplateID, UID::TemplateID_Random, UID::Property_Subobjects, InfoCell(ObjectType::Information...));
	bool Running = false;
	void FinishCallback(std::unique_ptr<CallbackMessage> ChildResult) override
	{
		if (ChildResult->Exception == UID::Exception_Success)
		{
			Progress++; // 无条件自增，因此不能放在if中
			switch (const UID Result = Iterate())
			{
			case UID::Exception_StillRunning:
				// 进度信息在Iterate中发送，而不在此处发，因为第一个子任务开始时不会运行到这里，但又必须在任务开始时发送当前进度。
				break;
			case UID::Exception_Success:
				Running = false;
				Parent->FinishCallback(std::move(ChildResult));
				break;
			default:
				Running = false;
				ChildResult->Exception = Result;
				ChildResult->StackTrace = {{StackLevel, ClassID, Progress}};
				Parent->FinishCallback(std::move(ChildResult));
			}
		}
		else
		{
			Running = false;
			ChildResult->StackTrace.push_back({StackLevel, ClassID, Progress});
			Parent->FinishCallback(std::move(ChildResult));
		}
	}
	UID Iterate()
	{
		uint8_t Start = Progress;
		while (Progress < sizeof...(ObjectType))
		{
			switch (const UID Result = SubPointers[Progress]->Start();)
			{
			default:
				return Result;
			case UID::Exception_StillRunning:
			{
				if (Progress == Start)
					Async_stream_IO::Send(CustomProgress(StackLevel, SubPointers[Progress]->ClassID, Start ? UID::Progress_Append : UID::Progress_Overwrite), ProgressPort);
				else
				{
					const uint8_t ProgressSize = sizeof(CustomProgress<UID>) + Progress - Start;
					std::unique_ptr<char[]> Buffer = std::make_unique_for_overwrite<char[]>(ProgressSize);
					*reinterpret_cast<CustomProgress<UID> *>(Buffer.get()) = CustomProgress(StackLevel, SubPointers[Progress]->ClassID, Start ? UID::Progress_Append : UID::Progress_Overwrite);
					UID *Pointer = reinterpret_cast<UID *>(Buffer.get() + sizeof(CustomProgress<UID>));
					while (++Start <= Progress)
						*Pointer++ = SubPointers[Start]->ClassID;
					Async_stream_IO::Send([Buffer = std::move(Buffer), ProgressSize](const std::move_only_function<void(const void *Message, uint8_t Size) const> &MessageSender)
										  { MessageSender(Buffer.get(), ProgressSize); }, ProgressPort);
				}
				return UID::Exception_StillRunning;
			}
			case UID::Exception_Success:
				Progress++;
			}
		}
		return UID::Exception_Success;
	}
	// 随机重复对象运行，每次开始时打乱顺序。
	template <uint16_t... Times>
	struct WithRepeat : ChildObject
	{
		WithRepeat(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel, UID ClassID = UID::TemplateID_RandomRepeat) : ChildObject(ProgressPort, Parent, StackLevel, ClassID), SubObjects(ObjectType(ProgressPort, this, StackLevel + 1)...)
		{
			ExpandIndex::TupleFillArray(SubObjects, SubPointers);
			ExpandIndex::TimesFillIndex<Times...>(ShuffleIndex);
		}
		virtual UID Start() override
		{
			if (Running)
				return UID::Exception_ObjectNotIdle;
			Progress = 0;
			std::shuffle(std::begin(ShuffleIndex), std::end(ShuffleIndex), Urng);
			std::fill(std::begin(ProgressMessage.RepeatsDone), std::end(ProgressMessage.RepeatsDone), 0);
			const UID Result = Iterate();
			Running = Result == UID::Exception_StillRunning;
			return Result;
		}
		UID Restore(std::span<const uint8_t> ProgressInfo) override
		{
			if (Running)
				return UID::Exception_ObjectNotIdle;
			ProgressMessage = *reinterpret_cast<const ProgressType *>(ProgressInfo.data() + 1);
			ProgressInfo = ProgressInfo.subspan(sizeof(ProgressMessage) + 1);
			Progress = std::accumulate(std::begin(ProgressMessage.RepeatsDone), std::end(ProgressMessage.RepeatsDone), 0);
			uint8_t *Pointer = ShuffleIndex;
			uint8_t Index;
			for (Index = 0; Index < sizeof...(ObjectType); ++Index)
				Pointer = std::fill_n(Pointer, ProgressMessage.RepeatsDone[Index], Index);
			for (Index = 0; Index < sizeof...(ObjectType); ++Index)
				if (SubPointers[Index]->ClassID == ProgressMessage.Current)
				{
					*Pointer++ = Index;
					break;
				}
			if (Index == sizeof...(ObjectType))
				return UID::Exception_ProgressObjectNotFound;
			ExpandIndex::LeftFillIndex<Times...>(Pointer, ProgressMessage.RepeatsDone);
			std::shuffle(Pointer, std::end(ShuffleIndex), Urng);
			const UID Result = ProgressInfo.empty() ? Iterate() : SubPointers[ShuffleIndex[Progress]]->Restore(ProgressInfo);
			Running = Result == UID::Exception_StillRunning;
			return Result;
		}
		UID Pause() override
		{
			return Running ? SubPointers[ShuffleIndex[Progress]]->Pause() : UID::Exception_ObjectNotRunning;
		}
		UID Continue() override
		{
			return Running ? SubPointers[ShuffleIndex[Progress]]->Continue() : UID::Exception_ObjectNotPaused;
		}
		UID Abort() override
		{
			if (Running)
			{
				Running = false;
				return SubPointers[ShuffleIndex[Progress]]->Abort();
			}
			else
				return UID::Exception_ObjectNotRunning;
		}
		OverrideGetInformation;

	protected:
		using ExpandIndex = ExpandSequence<std::make_integer_sequence<uint8_t, sizeof...(Times)>>;
#pragma pack(push, 1)
		struct ProgressType
		{
			uint16_t RepeatsDone[sizeof...(ObjectType)];
			UID Current;
		} ProgressMessage;
#pragma pack(pop)
		const std::tuple<ObjectType...> SubObjects;
		ChildObject *SubPointers[sizeof...(ObjectType)];
		uint8_t ShuffleIndex[Sum<Times...>()];
		uint16_t Progress;
		static constexpr auto Information PROGMEM = InfoStruct(UID::Property_TemplateID, UID::TemplateID_RandomRepeat, UID::Property_Subobjects, InfoCell(InfoStruct(UID::Property_ObjectInfo, ObjectType::Information, UID::Property_RepeatTime, Times)...));
		bool Running = false;
		void FinishCallback(std::unique_ptr<CallbackMessage> ChildResult) override
		{
			if (ChildResult->Exception == UID::Exception_Success)
			{
				Progress++;
				switch (const UID Result = Iterate())
				{
				case UID::Exception_StillRunning:
					// 进度信息在Iterate中发送，而不在此处发，因为第一个子任务开始时不会运行到这里，但又必须在任务开始时发送当前进度。
					break;
				case UID::Exception_Success:
					Running = false;
					Parent->FinishCallback(std::move(ChildResult));
					break;
				default:
					Running = false;
					ChildResult->Exception = Result;
					ChildResult->StackTrace = {{StackLevel, ClassID, Progress}};
					Parent->FinishCallback(std::move(ChildResult));
				}
			}
			else
			{
				Running = false;
				ChildResult->StackTrace.push_back({StackLevel, ClassID, Progress});
				Parent->FinishCallback(std::move(ChildResult));
			}
		}
		UID Iterate()
		{
			while (Progress < Sum<Times...>())
				switch (const UID Result = SubPointers[ShuffleIndex[Progress]]->Start())
				{
				default:
					return Result;
				case UID::Exception_StillRunning:
					ProgressMessage.Current = SubPointers[ShuffleIndex[Progress]]->ClassID;
					Async_stream_IO::Send(CustomProgress(StackLevel, ProgressMessage), ProgressPort);
					return UID::Exception_StillRunning;
				case UID::Exception_Success:
					ProgressMessage.RepeatsDone[ShuffleIndex[Progress++]]++;
				}
			return UID::Exception_Success;
		}
	};
};
// 按随机顺序执行每个对象各一次
template <typename... ObjectType>
struct Random : _Random<ObjectType...>
{
	using _Random<ObjectType...>::_Random;

	// 为对象附加自定义ID
	template <UID CustomID>
	struct WithID : _Random<ObjectType...>
	{
		WithID(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel) : _Random<ObjectType...>(ProgressPort, Parent, StackLevel, CustomID) {}
		OverrideGetInformation;

	protected:
		static constexpr auto Information PROGMEM = InfoStruct(UID::Property_TemplateID, CustomID, UID::Property_Subobjects, InfoCell(ObjectType::Information...));
	};

	// 随机穿插地重复执行每个对象指定次数
	template <uint16_t... Times>
	struct WithRepeat : _Random<ObjectType...>::template WithRepeat<Times...>
	{
		using _Random<ObjectType...>::template WithRepeat<Times...>::WithRepeat;

		// 为对象附加自定义ID
		template <UID CustomID>
		struct WithID : _Random<ObjectType...>::template WithRepeat<Times...>
		{
			WithID(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel) : _Random<ObjectType...>::template WithRepeat<Times...>(ProgressPort, Parent, StackLevel, CustomID) {}
			OverrideGetInformation;

		protected:
			static constexpr auto Information PROGMEM = InfoStruct(UID::Property_TemplateID, CustomID, UID::Property_Subobjects, InfoCell(InfoStruct(UID::Property_ObjectInfo, ObjectType::Information, UID::Property_RepeatTime, Times)...));
		};
	};
};

template <typename T, T Value, UID CustomID = UID::TemplateID_Signal>
struct Signal : ChildObject
{
	Signal(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel) : ChildObject(ProgressPort, Parent, StackLevel, CustomID) {}
	UID Start() override
	{
		Async_stream_IO::Send(SignalProgress(Value), ProgressPort);
		return UID::Exception_Success;
	}
	OverrideGetInformation;

protected:
	static constexpr auto Information PROGMEM = InfoStruct(UID::Property_TemplateID, CustomID, UID::Property_SignalValue, Value);
};

template <UID CustomID = UID::TemplateID_TrialStart>
using TrialStart = Signal<UID, UID::Signal_TrialStart, CustomID>;