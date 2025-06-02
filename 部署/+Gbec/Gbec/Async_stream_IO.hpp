#pragma once
#include <Cpp_Standard_Library.h>
#include <memory>
#include <span>
#include <unordered_map>
#include <Arduino.h>
namespace Async_stream_IO {
enum class Exception : uint8_t {
	Success,
	Port_occupied,
	Port_idle,
	Argument_message_incomplete,
	Corrupted_object_received,
};
struct _InterruptGuard {
	_InterruptGuard() {
		noInterrupts();
	}
	~_InterruptGuard() {
		if (InterruptEnabled)
			interrupts();
	}

protected:
	bool const InterruptEnabled =
#ifdef ARDUINO_ARCH_AVR
	  SREG & 1 << SREG_I
#endif
#ifdef ARDUINO_ARCH_SAM
	           !__get_PRIMASK()
#endif
	  ;
};
//轻量化的消息发送同步器。对象构造后将禁用中断，保证消息连续发出。此对象生存期内，用户必须直接使用BaseStream.write()方法发送数据，而不能使用Send；此方法发送的所有数据会被字节串联并视为单条消息。在对象析构前，用户必须恰好已发送Length字节的数据，不能多也不能少，否则行为未定义。对象析构时中断会恢复到构造前的状态。
struct SendSession : protected _InterruptGuard {
	SendSession(uint8_t Length, uint8_t ToPort, Stream& BaseStream);
};
template<size_t Plus, typename T>
struct _PlusAndPrepend;
template<size_t Plus, size_t... Values>
struct _PlusAndPrepend<Plus, std::index_sequence<Values...>> {
	using type = std::index_sequence<0, Plus + Values...>;
};

template<typename T>
struct _CumSum;
template<size_t First, size_t... Rest>
struct _CumSum<std::index_sequence<First, Rest...>> {
	using type = typename _PlusAndPrepend<First, typename _CumSum<std::index_sequence<Rest...>>::type>::type;
};
template<>
struct _CumSum<std::index_sequence<>> {
	using type = std::index_sequence<>;
};

template<typename Offsets>
struct _InvokeWithMemoryOffsets;
template<size_t... Offsets>
struct _InvokeWithMemoryOffsets<std::index_sequence<Offsets...>> {
	template<typename ReturnType, typename... ArgumentType>
	static ReturnType Invoke(const std::move_only_function<ReturnType(ArgumentType...) const>& Function, const uint8_t* Memory) {
		return Function(*reinterpret_cast<const ArgumentType*>(Memory + Offsets)...);
	}
};

template<typename...>
struct _TypesSize {
	static constexpr size_t value = 0;
};
template<typename Only>
struct _TypesSize<Only> {
	static constexpr size_t value = sizeof(Only);
};
template<typename First, typename... Rest>
struct _TypesSize<First, Rest...> {
	static constexpr size_t value = sizeof(First) + _TypesSize<Rest...>::value;
};
constexpr size_t ST = _TypesSize<int>::value;

template<typename Signature>
struct _FunctionListener;

template<typename>
struct _NotMof {
	using type = void;
};
template<typename T>
struct _NotMof<std::move_only_function<T>> {};
template<typename...>
struct _FirstNotStream {
	static constexpr bool value = true;
};
static void _CallableAsStream_f(Stream&);
template<typename T, typename = void>
struct _CallableAsStream_s {
	static constexpr bool value = false;
};
template<typename T>
struct _CallableAsStream_s<T, decltype(_CallableAsStream_f(*std::declval<T*>()))> {
	static constexpr bool value = true;
};
template<typename First, typename... Rest>
struct _FirstNotStream<First, Rest...> {
	static constexpr bool value = !_CallableAsStream_s<First>::value;
};
struct AsyncStream {
	Stream& BaseStream;
	AsyncStream(Stream& BaseStream)
	  : BaseStream(BaseStream) {}

	/*一般应在loop中调用此方法。它将实际执行所有排队中的监听器。
				所有被委托的Stream在此方法中实际被读。那些Stream的所有操作都应当托管给此方法，用户不应再直接访问那些Stream，否则行为未定义。
				用户在本库其它方法中提供的回调，将被此方法实际调用。调用这些函数时，中断将处于启用状态。即使在调用此方法前禁用了中断，也会被此方法重新启用。即使在回调中禁用了中断，也仅适用于本次回调，返回后又将重新启用中断。这是因为串口操作需要中断支持，因此必须启用中断。
				 */
	void ExecuteTransactionsInQueue();


	// 向远程ToPort发送指定Length的Message
	void Send(const void* Message, uint8_t Length, uint8_t ToPort) const;
	// 向远程ToPort发送指定Message。这个Message应当可以直接将底层字节发送给串口
	template<typename T, std::enable_if_t<!std::invocable<T> _CSL_Parentheses11, int> = 0>
	inline void Send(const T& Message, uint8_t ToPort) const {
		Send(&Message, sizeof(T), ToPort);
	}

	/*监听FromStream流的本地FromPort端口。当远程传来指向FromPort的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback，提供消息字节数。在那之前，调用方有义务维持Message指针有效。
				如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
				此监听是一次性的。一旦收到消息，在调用Callback之前，监听就会结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
				如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，并提供存储该消息所需的字节数。
				此方法保证Callback被调用时中断处于启用状态。
				 */
	Exception Receive(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, uint8_t FromPort);
	/*监听FromStream流的本地FromPort端口。当远程传来指向FromPort的消息时，调用Callback，提供一个临时接口MessageReader和MessageSize，用于读取消息内容。MessageReader在Callback返回后失效，因此调用方不应试图保存MessageReader。
				可以多次调用MessageReader，将消息拆分读取到不同的内存位置。但在Callback返回之前，必须不多不少恰好读入全部MessageSize字节，否则行为未定义。
				如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
				此监听是一次性的。一旦收到消息，在调用Callback之前，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
				此方法是最灵活的接收方法，可以在消息实际到来时再进行任何处理，而不必提前分配接收缓冲区。
				此方法保证Callback被调用时中断处于启用状态。
				*/
	Exception Receive(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, uint8_t FromPort);
	/*监听FromStream流的本地FromPort端口。当远程传来指向FromPort的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback，提供消息字节数。在那之前，调用方有义务维持Message指针有效。
				此监听是一次性的。一旦收到消息，在调用Callback之前，监听就会结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
				如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，并提供存储该消息所需的字节数。
				此方法保证Callback被调用时中断处于启用状态。
				 */
	uint8_t Receive(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback);
	/*自动分配一个空闲本地端口返回，监听FromStream流的那个端口。当远程传来指向FromPort的消息时，调用Callback，提供一个临时接口MessageReader和MessageSize，用于读取消息内容。MessageReader在Callback返回后失效，因此调用方不应试图保存MessageReader。
				可以多次调用MessageReader，将消息拆分读取到不同的内存位置。但在Callback返回之前，必须不多不少恰好读入全部MessageSize字节，否则行为未定义。
				此监听是一次性的。一旦收到消息，在调用Callback之前，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
				此方法是最灵活的接收方法，可以在消息实际到来时再进行任何处理，而不必提前分配接收缓冲区。
				此方法保证Callback被调用时中断处于启用状态。
				*/
	uint8_t Receive(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback);

	/*持续监听FromStream流的本地FromPort端口。当远程传来指向FromPort的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback，提供消息字节数。在那之前，调用方有义务维持Message指针有效。
				如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
				此监听是持续性的。无论收到多少消息，端口都不会被释放。这意味着，只有在调用方主动ReleasePort时，监听才会结束。
				如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，并提供存储该消息所需的字节数。
				此方法保证Callback被调用时中断处于启用状态。
				 */
	Exception Listen(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, uint8_t FromPort);
	/*持续监听FromStream流的本地FromPort端口。当远程传来指向FromPort的消息时，调用Callback，提供一个临时接口MessageReader和MessageSize，用于读取消息内容。MessageReader在Callback返回后失效，因此调用方不应试图保存MessageReader。
				可以多次调用MessageReader，将消息拆分读取到不同的内存位置。但在Callback返回之前，必须不多不少恰好读入全部MessageSize字节，否则行为未定义。
				如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
				此监听是持续性的。无论收到多少消息，端口都不会被释放。这意味着，只有在调用方主动ReleasePort时，监听才会结束。
				此方法是最灵活的接收方法，可以在消息实际到来时再进行任何处理，而不必提前分配接收缓冲区。
				此方法保证Callback被调用时中断处于启用状态。
				*/
	Exception Listen(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, uint8_t FromPort);
	/*持续监听FromStream流的本地FromPort端口。当远程传来指向FromPort的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback，提供消息字节数。在那之前，调用方有义务维持Message指针有效。
				此监听是持续性的。无论收到多少消息，端口都不会被释放。这意味着，只有在调用方主动ReleasePort时，监听才会结束。
				如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，并提供存储该消息所需的字节数。
				此方法保证Callback被调用时中断处于启用状态。
				 */
	uint8_t Listen(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback);
	/*自动分配一个空闲本地端口返回，持续监听FromStream流的那个端口。当远程传来指向FromPort的消息时，调用Callback，提供一个临时接口MessageReader和MessageSize，用于读取消息内容。MessageReader在Callback返回后失效，因此调用方不应试图保存MessageReader。
				可以多次调用MessageReader，将消息拆分读取到不同的内存位置。但在Callback返回之前，必须不多不少恰好读入全部MessageSize字节，否则行为未定义。
				此监听是持续性的。无论收到多少消息，端口都不会被释放。这意味着，只有在调用方主动ReleasePort时，监听才会结束。
				此方法是最灵活的接收方法，可以在消息实际到来时再进行任何处理，而不必提前分配接收缓冲区。
				此方法保证Callback被调用时中断处于启用状态。
				*/
	uint8_t Listen(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback);

	/*立即释放指定本地端口，取消任何Receive或Listen监听。如果端口未被监听，返回Port_idle。无论如何，此方法返回后即可以在被释放的本地端口上附加新的监听。
				此方法不能用于取消挂起的Send委托。本库不支持取消Send委托。
				 */
	Exception ReleasePort(uint8_t Port);

	/* 将任意可调用对象绑定到指定本地端口上作为服务，当收到消息时调用。如果本地端口被占用，返回Port_occupied。
				远程要调用此对象，需要将所有参数序列化拼接成一个连续的内存块，并且头部加上一个uint8_t的远程端口号用来接收返回值，然后将此消息发送到此函数绑定的本地端口。如果消息字节数不足，将向远程调用方发送Parameter_message_incomplete。
				Function的参数和返回值必须是平凡类型。
				此方法保证Function被调用时中断处于启用状态。
				*/
	template<typename T>
	inline Exception BindFunctionToPort(T&& Function, uint8_t Port) {
		return Listen(_FunctionListener<std::_FunctionSignature_t<T>>(std::move(Function), this), Port);
	}
	/* 将任意可调用对象绑定到流作为服务，当收到消息时调用。返回远程要调用此对象需要发送消息到的本地端口号。
				远程要调用此对象，需要将所有参数序列化拼接成一个连续的内存块，并且头部加上一个uint8_t的远程端口号用来接收返回值，然后将此消息发送到此函数绑定的本地端口。如果消息字节数不足，将向远程调用方发送Parameter_message_incomplete。
				Function的参数和返回值必须是平凡类型。
				此方法保证Function被调用时中断处于启用状态。
				*/
	template<typename T>
	inline uint8_t BindFunctionToPort(T&& Function) {
		return Listen(_FunctionListener<std::_FunctionSignature_t<T>>(std::move(Function), this));
	}
	/* 向指定流BaseStream远程调用指定RemotePort上的函数，传入参数Arguments。当远程函数返回时，调用Callback，提供调用结果Result和返回值ReturnValue。
				如果远程端口未被监听，Callback将不会被调用。
				如果Result为Corrupted_object_received，说明收到了意外的返回值。这可能是因为远程函数返回值的类型与预期不符，或者有其它远程对象向本地端口发送了垃圾信息。此次远程调用将被废弃。
				如果Result为Parameter_message_incomplete，说明远程函数接收到的参数不完整。这可能是因为远程函数的参数类型与预期不符，或者有其它本地对象向远程端口发送了垃圾信息。此次远程调用将被废弃。
				ReturnType和ArgumentType必须是平凡类型。
				此方法保证Callback被调用时中断处于启用状态。
				*/
	template<typename ReturnType, typename... ArgumentType>
	void RemoteInvoke(uint8_t RemotePort, std::move_only_function<void(Exception Result, ReturnType ReturnValue) const>&& Callback, Stream& BaseStream, ArgumentType... Arguments) {
		SendSession const Session{ sizeof(uint8_t) + _TypesSize<ArgumentType...>::value, RemotePort, BaseStream };
		BaseStream.write(static_cast<char>(Receive([Callback = std::move(Callback)](const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) {
			const std::unique_ptr<char[]> Message = std::make_unique_for_overwrite<char[]>(MessageSize);
			MessageReader(Message.get(), MessageSize);
			if (MessageSize < sizeof(Exception))
				Callback(Exception::Corrupted_object_received, {});
			else if (MessageSize < sizeof(Exception) + sizeof(ReturnType))
				Callback(*reinterpret_cast<const Exception*>(Message.get()), {});
			else
				Callback(*reinterpret_cast<const Exception*>(Message.get()), *reinterpret_cast<const ReturnType*>(reinterpret_cast<const Exception*>(Message.get()) + 1));
		},
		                                           RemotePort)));
		size_t const Written[] = { BaseStream.write(reinterpret_cast<char*>(&Arguments), sizeof(Arguments))... };
	}
	/* 向指定流BaseStream远程调用指定RemotePort上的函数，传入参数Arguments。当远程函数返回时，调用Callback，提供调用结果Result和返回值ReturnValue。
				如果远程端口未被监听，Callback将不会被调用。
				如果Result为Corrupted_object_received，说明收到了意外的返回值。这可能是因为远程函数返回值的类型与预期不符，或者有其它远程对象向本地端口发送了垃圾信息。此次远程调用将被废弃。
				如果Result为Parameter_message_incomplete，说明远程函数接收到的参数不完整。这可能是因为远程函数的参数类型与预期不符，或者有其它本地对象向远程端口发送了垃圾信息。此次远程调用将被废弃。
				ReturnType和ArgumentType必须是平凡类型。
				此方法保证Callback被调用时中断处于启用状态。
				*/
	template<typename CallbackType, typename... ArgumentType>
	inline typename _NotMof<CallbackType>::type RemoteInvoke(uint8_t RemotePort, const CallbackType& Callback, Stream& BaseStream, ArgumentType... Arguments) {
		RemoteInvoke(RemotePort, std::move_only_function<std::_FunctionSignature_t<CallbackType>>(Callback), Arguments...);
	}
	/* 向Serial远程调用指定RemotePort上的函数，传入参数Arguments。当远程函数返回时，调用Callback，提供调用结果Result和返回值ReturnValue。
				如果远程端口未被监听，Callback将不会被调用。
				如果Result为Corrupted_object_received，说明收到了意外的返回值。这可能是因为远程函数返回值的类型与预期不符，或者有其它远程对象向本地端口发送了垃圾信息。此次远程调用将被废弃。
				如果Result为Parameter_message_incomplete，说明远程函数接收到的参数不完整。这可能是因为远程函数的参数类型与预期不符，或者有其它本地对象向远程端口发送了垃圾信息。此次远程调用将被废弃。
				ReturnType和ArgumentType必须是平凡类型。
				此方法保证Callback被调用时中断处于启用状态。
				*/
	template<typename ReturnType, typename... ArgumentType>
	inline std::enable_if_t<_FirstNotStream<ArgumentType...>::value> RemoteInvoke(uint8_t RemotePort, std::move_only_function<void(Exception Result, ReturnType ReturnValue) const>&& Callback, ArgumentType... Arguments) {
		RemoteInvoke(RemotePort, std::move(Callback), Serial, Arguments...);
	}
	/* 向Serial远程调用指定RemotePort上的函数，传入参数Arguments。当远程函数返回时，调用Callback，提供调用结果Result和返回值ReturnValue。
				如果远程端口未被监听，Callback将不会被调用。
				如果Result为Corrupted_object_received，说明收到了意外的返回值。这可能是因为远程函数返回值的类型与预期不符，或者有其它远程对象向本地端口发送了垃圾信息。此次远程调用将被废弃。
				如果Result为Parameter_message_incomplete，说明远程函数接收到的参数不完整。这可能是因为远程函数的参数类型与预期不符，或者有其它本地对象向远程端口发送了垃圾信息。此次远程调用将被废弃。
				ReturnType和ArgumentType必须是平凡类型。
				此方法保证Callback被调用时中断处于启用状态。
				*/
	template<typename CallbackType, typename... ArgumentType>
	inline typename _NotMof<CallbackType>::type RemoteInvoke(uint8_t RemotePort, const CallbackType& Callback, ArgumentType... Arguments) {
		RemoteInvoke(RemotePort, std::move_only_function<std::_FunctionSignature_t<CallbackType>>(Callback), Serial, Arguments...);
	}
	std::unordered_map<uint8_t, std::move_only_function<void() const>> _Listeners;
};
template<typename ReturnType, typename... ArgumentType>
struct _FunctionListener<ReturnType(ArgumentType...) const> {
	_FunctionListener(std::move_only_function<ReturnType(ArgumentType...) const>&& Function, AsyncStream* BaseStream)
	  : Function(std::move(Function)), BaseStream(BaseStream) {
	}
	void operator()(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const {
		const std::unique_ptr<uint8_t[]> Arguments = std::make_unique_for_overwrite<uint8_t[]>(MessageSize);
		MessageReader(Arguments.get(), MessageSize);
		constexpr uint8_t ArgumentSize = _TypesSize<ArgumentType...>::value + sizeof(uint8_t);
		if (MessageSize == ArgumentSize) {
#pragma pack(push, 1)
			struct ReturnMessage {
				Exception Result;
				ReturnType ReturnValue;
			};
#pragma pack(pop)
			BaseStream->Send(ReturnMessage{ Exception::Success, _InvokeWithMemoryOffsets<typename _CumSum<std::index_sequence<sizeof(ArgumentType)...>>::type>::Invoke(Function, Arguments.get() + 1) }, Arguments[0]);
		} else
			BaseStream->Send(Exception::Argument_message_incomplete, Arguments[0]);
	}

protected:
	std::move_only_function<ReturnType(ArgumentType...) const> Function;  //不能const，否则整个对象无法移动了
	AsyncStream*const BaseStream;
};
}