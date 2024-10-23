#pragma once
#include "UID.h"
#include "Async_stream_IO.hpp"
#include <unordered_map>
#include <random>
#include <dynarray>
#include <span>
#include <utility>

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
	UID Type = UID::Progress_Custom;
	uint8_t StackLevel;
	T Progress;
	constexpr CustomProgress(uint8_t StackLevel, T Progress) : StackLevel(StackLevel), Progress(Progress) {}
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
	virtual UID Start() { return UID::Exception_Success; }
	virtual UID Restore(std::span<const char> ProgressInfo) { return UID::Exception_Success; }
	// 支持重复的Object应override此方法，重复执行Times次，返回Exception_Success。
	virtual UID Repeat(uint16_t Times) { return UID::Exception_Success; }
	// 支持暂停的Object应override此方法，暂停当前执行，返回Exception_Success。调用方应检查返回值判断当前Object是否支持暂停，在不支持暂停的情况下采取其它措施实现暂停。
	virtual UID Pause() { return UID::Exception_MethodNotSupported; }
	// 支持暂停的Object应override此方法，继续上次暂停的执行。
	virtual UID Continue() { return UID::Exception_MethodNotSupported; }
	// 支持废弃的Object应override此方法，废弃当前执行，返回Exception_Success。调用方应检查返回值判断当前Object是否支持废弃，在不支持废弃的情况下采取其它措施实现废弃。
	virtual UID Abort() { return UID::Exception_MethodNotSupported; }
	// 支持获取信息的Object应override此方法
	virtual UID GetInformation(std::dynarray<char> &Container) { return UID::Exception_MethodNotSupported; }
	virtual ~Object() {}
	virtual void FinishCallback(std::unique_ptr<CallbackMessage> Message) = 0;

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
	UID Restore(std::span<const char> ProgressInfo) override
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
	virtual ~ChildObject() {}

protected:
	void FinishCallback(std::unique_ptr<CallbackMessage> Message) override
	{
		Parent->FinishCallback(std::move(Message));
	}
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
#define ParameterPackExpand(Expression) int _[] = {(Expression, 0)...};

template <typename TupleType, uint16_t... Repeat>
struct TupleFillArray_A
{
	template <typename = std::make_index_sequence<sizeof...(Repeat)>>
	struct TupleFillArray_B;
	template <size_t... Index>
	struct TupleFillArray_B<std::index_sequence<Index...>>
	{
		static void Fill(const TupleType &Tuple, const Object **Array)
		{
			ParameterPackExpand(Array = std::fill_n(Array, Repeat, &std::get<Index>(Tuple)));
		}
	};
};
template <typename TupleType, uint16_t... Repeat>
inline void TupleFillArray(const TupleType &Tuple, const Object **Array)
{
	TupleFillArray_A<TupleType, Repeat...>::TupleFillArray_B<>::Fill(Tuple, Array);
}
template <typename>
struct TupleFillArray_C;
template <size_t... Index>
struct TupleFillArray_C<std::index_sequence<Index...>>
{
	template <typename TupleType>
	static void Fill(const TupleType &Tuple, const Object **Array)
	{
		ParameterPackExpand(Array[Index] = &std::get<Index>(Tuple));
	}
};
template <typename... T>
inline void TupleFillArray(const std::tuple<T...> &Tuple, const Object **Array)
{
	TupleFillArray_C<std::index_sequence_for<T...>>::Fill(Tuple, Array);
}

// 常用对象模板

// 顺序依次执行所有的Object
template <typename... ObjectType>
struct Sequential : ChildObject
{
	Sequential(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel) : ChildObject(ProgressPort, Parent, StackLevel, UID::TemplateID_Sequential), SubObjects(ObjectType(ProgressPort, this, StackLevel + 1)...)
	{
		TupleFillArray(SubObjects, SubPointers);
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
	UID Restore(std::span<const char> ProgressInfo) override
	{
		if (Running)
			return UID::Exception_ObjectNotIdle;
		Progress = *reinterpret_cast<const uint8_t *>(ProgressInfo.data());
		ProgressInfo = ProgressInfo.subspan(sizeof(Progress));
		const UID Result = ProgressInfo.empty() ? Iterate() : SubPointers[Progress]->Restore(ProgressInfo);
		Running = Result == UID::Exception_StillRunning;
		return Result;
	}
	UID Repeat(uint16_t Times) override
	{
		return UID::Exception_MethodNotSupported;
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
			if (StackLevel < UINT8_MAX)
				Async_stream_IO::Send(CustomProgress(StackLevel, Progress), ProgressPort);
			switch (const UID Result = Iterate())
			{
			case UID::Exception_StillRunning:
				Running = true;
				break;
			case UID::Exception_Success:
				Running = false;
				Parent->FinishCallback(std::move(ChildResult));
				break;
			default:
				Running = false;
				ChildResult->Exception = Result;
				ChildResult->StackTrace = {{StackLevel, UID::TemplateID_Sequential, Progress}};
				Parent->FinishCallback(std::move(ChildResult));
			}
		}
		else
		{
			Running = false;
			ChildResult->StackTrace.push_back({StackLevel, UID::TemplateID_Sequential, Progress});
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
		WithRepeat(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel) : ChildObject(ProgressPort, Parent, StackLevel, UID::TemplateID_SequentialRepeat), SubObjects(ObjectType(ProgressPort, this, StackLevel + 1)...)
		{
			TupleFillArray<decltype(SubObjects), Times...>(SubObjects, SubPointers);
		}
		virtual UID Start() override
		{
			if (Running)
				return UID::Exception_ObjectNotIdle;
			Progress = 0;
			const UID Result = Iterate();
			Running = Result == UID::Exception_StillRunning;
			return Result;
		}
		UID Restore(std::span<const char> ProgressInfo) override
		{
			if (Running)
				return UID::Exception_ObjectNotIdle;
			Progress = *reinterpret_cast<const uint16_t *>(ProgressInfo.data());
			ProgressInfo = ProgressInfo.subspan(sizeof(Progress));
			const UID Result = ProgressInfo.empty() ? Iterate() : SubPointers[Progress]->Restore(ProgressInfo);
			Running = Result == UID::Exception_StillRunning;
			return Result;
		}
		UID Repeat(uint16_t Times) override
		{
			return UID::Exception_MethodNotSupported;
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
				if (StackLevel < UINT8_MAX)
					Async_stream_IO::Send(CustomProgress(StackLevel, Progress), ProgressPort);
				switch (const UID Result = Iterate())
				{
				case UID::Exception_StillRunning:
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

// 随机顺序执行Object。
#ifdef ARDUINO_ARCH_AVR
using ArchUrng = std::ArduinoUrng;
#else
using ArchUrng = std::TrueUrng;
#endif
extern ArchUrng Urng;
template <typename... ObjectType>
struct Random : ChildObject
{
	Random(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel) : ChildObject(ProgressPort, Parent, StackLevel, UID::TemplateID_Random), SubObjects(ObjectType(ProgressPort, this, StackLevel + 1)...)
	{
		TupleFillArray(SubObjects, SubPointers);
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
	UID Restore(std::span<const char> ProgressInfo) override
	{
		if (Running)
			return UID::Exception_ObjectNotIdle;
		Progress = *reinterpret_cast<const uint8_t *>(ProgressInfo.data());
		ProgressInfo = ProgressInfo.subspan(sizeof(Progress));
		const UID *Pointer = reinterpret_cast<const UID *>(ProgressInfo.data());
		for (uint8_t P1 = 0; P1 < Progress; P1++)
		{
			uint8_t P2;
			for (P2 = P1; P2 < sizeof...(ObjectType); P2++)
				if (Pointer[P1] == SubPointers[P2]->ClassID)
				{
					std::swap(SubPointers[P1], SubPointers[P2]);
					break;
				}
			if (P2 >= sizeof...(ObjectType))
				return UID::Exception_ProgressObjectNotFound;
		}
		ProgressInfo = ProgressInfo.subspan(Progress * sizeof(UID));
		std::shuffle(SubPointers + Progress, SubPointers + sizeof...(ObjectType), Urng);
		const UID Result = ProgressInfo.empty() ? Iterate() : SubPointers[Progress]->Restore(ProgressInfo);
		Running = Result == UID::Exception_StillRunning;
		return Result;
	}
	UID Repeat(uint16_t Times) override
	{
		return UID::Exception_MethodNotSupported;
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
	static constexpr UID TemplateID = UID::TemplateID_Random;

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
			if (StackLevel < UINT8_MAX)
			{
				//TODO：这里需要发送不定长的完整进度信息，因为接收方假定你的进度信息是完整的
			}
				Async_stream_IO::Send(CustomProgress(StackLevel, SubPointers[Progress]->ClassID), ProgressPort);
			Progress++; // 无条件自增，因此不能放在if中
			switch (const UID Result = Iterate())
			{
			case UID::Exception_StillRunning:
				Running = true;
				break;
			case UID::Exception_Success:
				Running = false;
				Parent->FinishCallback(std::move(ChildResult));
				break;
			default:
				Running = false;
				ChildResult->Exception = Result;
				ChildResult->StackTrace = {{StackLevel, UID::TemplateID_Sequential, Progress}};
				Parent->FinishCallback(std::move(ChildResult));
			}
		}
		else
		{
			Running = false;
			ChildResult->StackTrace.push_back({StackLevel, UID::TemplateID_Sequential, Progress});
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
		WithRepeat(uint8_t ProgressPort, Object *Parent, uint8_t StackLevel) : ChildObject(ProgressPort, Parent, StackLevel)
		{
			const Object **Pointer = SubObjects;
			const uint8_t ChildStackLevel = StackLevel + 1;
			ParameterPackExpand(Pointer = std::fill_n(Pointer, Times, new ObjectType(ProgressPort, this, ChildStackLevel)));
		}
		virtual UID Start() override
		{
			if (Running)
				return UID::Exception_ObjectNotIdle;
			Progress = 0;
			const UID Result = Iterate();
			Running = Result == UID::Exception_StillRunning;
			return Result;
		}
		UID Restore(std::span<const char> ProgressInfo) override
		{
			if (Running)
				return UID::Exception_ObjectNotIdle;
			Progress = *reinterpret_cast<const uint16_t *>(ProgressInfo.data());
			if (Progress >= Sum<Times...>())
				return UID::Exception_Success;
			ProgressInfo = ProgressInfo.subspan(sizeof(Progress));
			const UID Result = ProgressInfo.empty() ? Iterate() : SubObjects[Progress]->Restore(ProgressInfo);
			Running = Result == UID::Exception_StillRunning;
			return Result;
		}
		UID Repeat(uint16_t Times) override
		{
			return UID::Exception_MethodNotSupported;
		}
		UID Pause() override
		{
			return Running ? SubObjects[Progress]->Pause() : UID::Exception_ObjectNotRunning;
		}
		UID Continue() override
		{
			return Running ? SubObjects[Progress]->Continue() : UID::Exception_ObjectNotPaused;
		}
		UID Abort() override
		{
			if (Running)
			{
				Running = false;
				return SubObjects[Progress]->Abort();
			}
			else
				return UID::Exception_ObjectNotRunning;
		}
		virtual ~WithRepeat()
		{
			Object *const *Pointer = SubObjects;
			const Object *const *const _[] = {(delete *Pointer, Pointer += Times)...};
		}
		OverrideGetInformation;

	protected:
		uint16_t Progress;
		Object *SubObjects[Sum<Times...>()];
		static constexpr auto Information PROGMEM = InfoStruct(UID::Property_TemplateID, UID::TemplateID_SequentialRepeat, UID::Property_Subobjects, InfoCell(InfoStruct(UID::Property_ObjectInfo, ObjectType::Information, UID::Property_RepeatTime, Times)...));
		bool Running = false;
		void FinishCallback(std::unique_ptr<CallbackMessage> ChildResult) override
		{
			if (ChildResult->Exception == UID::Exception_Success)
			{
				Progress++;
				if (StackLevel < UINT8_MAX)
					Async_stream_IO::Send(CustomProgress(StackLevel, Progress), ProgressPort);
				switch (const UID Result = Iterate())
				{
				case UID::Exception_StillRunning:
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
				ChildResult->StackTrace.push_back({StackLevel, UID::TemplateID_Sequential, Progress});
				Parent->FinishCallback(std::move(ChildResult));
			}
		}
		UID Iterate()
		{
			while (Progress < Sum<Times...>())
				switch (const UID Result = SubObjects[Progress]->Start();)
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