#pragma once
#include <Cpp_Standard_Library.h>
#include <memory>
#include <span>
#include <Arduino.h>
namespace Async_stream_IO
{
	enum class Exception : uint8_t
	{
		Success,
		Port_occupied,
		Port_idle,
		Argument_message_incomplete,
		Corrupted_object_received,
	};

	/*一般应在loop中调用此方法。它将实际执行所有排队中的事务，包括发送消息和调用监听器。
	所有被委托的Stream在此方法中实际被读写。那些Stream的所有操作都应当托管给此方法，用户不应再直接访问那些Stream，否则行为未定义。
	用户在本库其它方法中提供的回调，将被此方法实际调用。调用这些函数时，中断将处于启用状态。即使在调用此方法前禁用了中断，也会被此方法重新启用。即使在回调中禁用了中断，也仅适用于本次回调，返回后又将重新启用中断。这是因为串口操作需要中断支持，因此必须启用中断。
	 */
	void ExecuteTransactionsInQueue();

	// 委托下次ExecuteTransactionsInQueue时，向远程ToPort发送指定Length的Message。调用方应确保那时Message指针仍然有效。
	void Send(const void* Message, uint8_t Length, uint8_t ToPort, Stream& ToStream = Serial);
	/* 委托下次ExecuteTransactionsInQueue时，调用Callback，提供一个临时接口MessageSender，用于向远程发送消息。MessageSender在Callback返回后失效，因此调用方不应试图保存MessageSender。
	调用方可以多次使用该接口，向远程ToPort发送任意Size的Message任意多条。但请注意，每次调用MessageSender，在逻辑上都将视为发送一条单独的消息，而不会合并成一条发送。如果需要合并成一条发送，请在Callback中自行实现消息合并逻辑。
	此方法是最灵活的发送方法，可用于发送临时生成的消息，而不必将消息持久存储以等待发送。
	此方法保证Callback被调用时中断处于启用状态。
	 */
	void Send(std::move_only_function<void(const std::move_only_function<void(const void* Message, uint8_t Size) const>& MessageSender) const>&& Callback, uint8_t ToPort, Stream& ToStream = Serial);
	// 委托下次ExecuteTransactionsInQueue时，向远程ToPort发送指定Message。这个Message应当可以直接将底层字节发送给串口。
	template<typename T, std::enable_if_t<!std::invocable<T>_CSL_Parentheses11, int> = 0>
	inline void Send(const T& Message, uint8_t ToPort, Stream& ToStream = Serial)
	{
		Send([Message = Message](const std::move_only_function<void(const void* Message, uint8_t Size) const>& MessageSender)
			{
				MessageSender(&Message, sizeof(Message));
			}, ToPort, ToStream);
	}

	/*监听FromStream流的本地FromPort端口。当远程传来指向FromPort的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback，提供消息字节数。在那之前，调用方有义务维持Message指针有效。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	此监听是一次性的。一旦收到消息，在调用Callback之前，监听就会结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，并提供存储该消息所需的字节数。
	此方法保证Callback被调用时中断处于启用状态。
	 */
	Exception Receive(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream = Serial);
	/*监听FromStream流的本地FromPort端口。当远程传来指向FromPort的消息时，调用Callback，提供一个临时接口MessageReader和MessageSize，用于读取消息内容。MessageReader在Callback返回后失效，因此调用方不应试图保存MessageReader。
	可以多次调用MessageReader，将消息拆分读取到不同的内存位置。但在Callback返回之前，必须不多不少恰好读入全部MessageSize字节，否则行为未定义。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	此监听是一次性的。一旦收到消息，在调用Callback之前，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	此方法是最灵活的接收方法，可以在消息实际到来时再进行任何处理，而不必提前分配接收缓冲区。
	此方法保证Callback被调用时中断处于启用状态。
	*/
	Exception Receive(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream = Serial);
	/*自动分配一个空闲本地端口返回，监听FromStream流的那个端口。当远程传来指向FromPort的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback，提供消息字节数。在那之前，调用方有义务维持Message指针有效。
	此监听是一次性的。一旦收到消息，在调用Callback之前，监听就会结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，并提供存储该消息所需的字节数。
	此方法保证Callback被调用时中断处于启用状态。
	 */
	uint8_t Receive(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, Stream& FromStream = Serial);
	/*自动分配一个空闲本地端口返回，监听FromStream流的那个端口。当远程传来指向FromPort的消息时，调用Callback，提供一个临时接口MessageReader和MessageSize，用于读取消息内容。MessageReader在Callback返回后失效，因此调用方不应试图保存MessageReader。
	可以多次调用MessageReader，将消息拆分读取到不同的内存位置。但在Callback返回之前，必须不多不少恰好读入全部MessageSize字节，否则行为未定义。
	此监听是一次性的。一旦收到消息，在调用Callback之前，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	此方法是最灵活的接收方法，可以在消息实际到来时再进行任何处理，而不必提前分配接收缓冲区。
	此方法保证Callback被调用时中断处于启用状态。
	*/
	uint8_t Receive(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, Stream& FromStream = Serial);

	/*持续监听FromStream流的本地FromPort端口。当远程传来指向FromPort的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback，提供消息字节数。在那之前，调用方有义务维持Message指针有效。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	此监听是持续性的。无论收到多少消息，端口都不会被释放。这意味着，只有在调用方主动ReleasePort时，监听才会结束。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，并提供存储该消息所需的字节数。
	此方法保证Callback被调用时中断处于启用状态。
	 */
	Exception Listen(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream = Serial);
	/*持续监听FromStream流的本地FromPort端口。当远程传来指向FromPort的消息时，调用Callback，提供一个临时接口MessageReader和MessageSize，用于读取消息内容。MessageReader在Callback返回后失效，因此调用方不应试图保存MessageReader。
	可以多次调用MessageReader，将消息拆分读取到不同的内存位置。但在Callback返回之前，必须不多不少恰好读入全部MessageSize字节，否则行为未定义。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	此监听是持续性的。无论收到多少消息，端口都不会被释放。这意味着，只有在调用方主动ReleasePort时，监听才会结束。
	此方法是最灵活的接收方法，可以在消息实际到来时再进行任何处理，而不必提前分配接收缓冲区。
	此方法保证Callback被调用时中断处于启用状态。
	*/
	Exception Listen(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, uint8_t FromPort, Stream& FromStream = Serial);
	/*自动分配一个空闲本地端口返回，持续监听FromStream流的那个端口。当远程传来指向FromPort的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback，提供消息字节数。在那之前，调用方有义务维持Message指针有效。
	此监听是持续性的。无论收到多少消息，端口都不会被释放。这意味着，只有在调用方主动ReleasePort时，监听才会结束。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，并提供存储该消息所需的字节数。
	此方法保证Callback被调用时中断处于启用状态。
	 */
	uint8_t Listen(void* Message, uint8_t Capacity, std::move_only_function<void(uint8_t MessageSize) const>&& Callback, Stream& FromStream = Serial);
	/*自动分配一个空闲本地端口返回，持续监听FromStream流的那个端口。当远程传来指向FromPort的消息时，调用Callback，提供一个临时接口MessageReader和MessageSize，用于读取消息内容。MessageReader在Callback返回后失效，因此调用方不应试图保存MessageReader。
	可以多次调用MessageReader，将消息拆分读取到不同的内存位置。但在Callback返回之前，必须不多不少恰好读入全部MessageSize字节，否则行为未定义。
	此监听是持续性的。无论收到多少消息，端口都不会被释放。这意味着，只有在调用方主动ReleasePort时，监听才会结束。
	此方法是最灵活的接收方法，可以在消息实际到来时再进行任何处理，而不必提前分配接收缓冲区。
	此方法保证Callback被调用时中断处于启用状态。
	*/
	uint8_t Listen(std::move_only_function<void(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) const>&& Callback, Stream& FromStream = Serial);

	/*立即释放指定本地端口，取消任何Receive或Listen监听。如果端口未被监听，返回Port_idle。无论如何，此方法返回后即可以在被释放的本地端口上附加新的监听。
	此方法不能用于取消挂起的Send委托。本库不支持取消Send委托。
	 */
	Exception ReleasePort(uint8_t Port);

	template <size_t Plus, typename T>
	struct _PlusAndPrepend;
	template <size_t Plus, size_t... Values>
	struct _PlusAndPrepend<Plus, std::index_sequence<Values...>>
	{
		using type = std::index_sequence<0, Plus + Values...>;
	};

	template <typename T>
	struct _CumSum;
	template <size_t First, size_t... Rest>
	struct _CumSum<std::index_sequence<First, Rest...>>
	{
		using type = typename _PlusAndPrepend<First, typename _CumSum<std::index_sequence<Rest...>>::type>::type;
	};
	template <>
	struct _CumSum<std::index_sequence<>>
	{
		using type = std::index_sequence<>;
	};

	template <typename Offsets>
	struct _InvokeWithMemoryOffsets;
	template <size_t... Offsets>
	struct _InvokeWithMemoryOffsets<std::index_sequence<Offsets...>>
	{
		template <typename ReturnType, typename... ArgumentType>
		static ReturnType Invoke(const std::move_only_function<ReturnType(ArgumentType...) const>& Function, const uint8_t* Memory)
		{
			return Function(*reinterpret_cast<const ArgumentType*>(Memory + Offsets)...);
		}
	};

	template <typename First, typename... Rest>
	constexpr size_t _TypesSize()
	{
		return sizeof(First) + _TypesSize<Rest...>();
	}
	template <typename Only>
	constexpr size_t _TypesSize()
	{
		return sizeof(Only);
	}

	template <typename Signature>
	struct _FunctionListener;
	template <typename ReturnType, typename... ArgumentType>
	struct _FunctionListener<ReturnType(ArgumentType...)>
	{
		_FunctionListener(std::move_only_function<ReturnType(ArgumentType...) const>&& Function, Stream& ToStream) : Function(std::move(Function)), ToStream(ToStream) {}
		void operator()(const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize)
		{
			const std::unique_ptr<uint8_t[]> Arguments = std::make_unique_for_overwrite<uint8_t[]>(MessageSize);
			MessageReader(Arguments.get(), MessageSize);
			constexpr uint8_t ArgumentSize = _TypesSize<ArgumentType...>() + sizeof(uint8_t);
			if (MessageSize == ArgumentSize)
			{
#pragma pack(push, 1)
				struct ReturnMessage
				{
					const Exception Result = Exception::Success;
					const ReturnType ReturnValue;
					void operator()(const std::move_only_function<void(const void* Message, uint8_t Size) const>& MessageSender) const
					{
						MessageSender(this, sizeof(*this));
					}
					constexpr ReturnMessage(ReturnType ReturnValue) : ReturnValue(std::move(ReturnValue)) {}
				};
#pragma pack(pop)
				Send([RM = ReturnMessage{ _InvokeWithMemoryOffsets<typename _CumSum<std::index_sequence<sizeof(ArgumentType)...>>::type>::Invoke<ReturnType, ArgumentType...>(Function, Arguments.get() + 1) }](const std::move_only_function<void(const void* Message, uint8_t Size) const>& MessageSender)
					{ MessageSender(&RM, sizeof(RM)); }, Arguments[0], ToStream);
			}
			else
			{
				static constexpr Exception Argument_message_incomplete = Exception::Argument_message_incomplete;
				Send(&Argument_message_incomplete, sizeof(Exception), Arguments[0], ToStream);
			}
		}

	protected:
		const std::move_only_function<ReturnType(ArgumentType...) const> Function;
		Stream& ToStream;
	};
	template <typename T>
	_FunctionListener(const T&, Stream&) -> _FunctionListener<typename std::_FunctionSignature<T>::type>;

	/* 将任意可调用对象绑定到指定本地端口上，当收到消息时调用。如果本地端口被占用，返回Port_occupied。
	远程要调用此对象，需要将所有参数序列化拼接称一个连续的内存块，并且头部加上一个uint8_t的远程端口号用来接收返回值，然后将此消息发送到此函数绑定的本地端口。如果消息字节数不足，将向远程调用方发送Parameter_message_incomplete。
	Function的参数和返回值必须是平凡类型。
	此方法保证Function被调用时中断处于启用状态。
	*/
	template <typename T>
	inline Exception BindFunctionToPort(T&& Function, uint8_t Port, Stream& ToStream = Serial)
	{
		return Listen(_FunctionListener(std::move(Function), ToStream), Port, ToStream);
	}
	/* 将任意可调用对象绑定到流，当收到消息时调用。返回远程要调用此对象需要发送消息到的本地端口号。
	远程要调用此对象，需要将所有参数序列化拼接称一个连续的内存块，并且头部加上一个uint8_t的远程端口号用来接收返回值，然后将此消息发送到此函数绑定的本地端口。如果消息字节数不足，将向远程调用方发送Parameter_message_incomplete。
	Function的参数和返回值必须是平凡类型。
	此方法保证Function被调用时中断处于启用状态。
	*/
	template <typename T>
	inline uint8_t BindFunctionToPort(T&& Function, Stream& ToStream = Serial)
	{
		return Listen(_FunctionListener(std::move(Function), ToStream), ToStream);
	}

	/* 向指定流ToStream远程调用指定RemotePort上的函数，传入参数Arguments。当远程函数返回时，调用Callback，提供调用结果Result和返回值ReturnValue。
	如果远程端口未被监听，Callback将不会被调用。
	如果Result为Corrupted_object_received，说明收到了意外的返回值。这可能是因为远程函数返回值的类型与预期不符，或者有其它远程对象向本地端口发送了垃圾信息。此次远程调用将被废弃。
	如果Result为Parameter_message_incomplete，说明远程函数接收到的参数不完整。这可能是因为远程函数的参数类型与预期不符，或者有其它本地对象向远程端口发送了垃圾信息。此次远程调用将被废弃。
	ReturnType和ArgumentType必须是平凡类型。
	此方法保证Callback被调用时中断处于启用状态。
	*/
	template <typename ReturnType, typename... ArgumentType>
	void RemoteInvoke(uint8_t RemotePort, std::move_only_function<void(Exception Result, ReturnType ReturnValue) const>&& Callback, Stream& ToStream, ArgumentType... Arguments)
	{
		constexpr size_t MessageSize = sizeof(uint8_t) + _TypesSize<ArgumentType...>();
		std::unique_ptr<uint8_t[]> Message = std::make_unique_for_overwrite<uint8_t[]>(MessageSize);
		Message[0] = Receive([Callback = std::move(Callback)](const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize)
			{
				const std::unique_ptr<char[]>Message = std::make_unique_for_overwrite<char[]>(MessageSize);
				MessageReader(Message.get(), MessageSize);
				if (MessageSize < sizeof(Exception))
					Callback(Exception::Corrupted_object_received, {});
				else if (MessageSize < sizeof(Exception) + sizeof(ReturnType))
					Callback(*reinterpret_cast<const Exception*>(Message.get()), {});
				else
					Callback(*reinterpret_cast<const Exception*>(Message.get()), *reinterpret_cast<const ReturnType*>(reinterpret_cast<const Exception*>(Message.get()) + 1)); }, RemotePort, ToStream);
		char* Pointer = reinterpret_cast<char*>(Message.get() + 1);
		char* _[] = { (*reinterpret_cast<ArgumentType*>(Pointer) = Arguments, Pointer += sizeof(Arguments))... };
		Send([Message = std::move(Message)](const std::move_only_function<void(const void* Message, uint8_t Size) const>& MessageSender)
			{
				MessageSender(Message.get(), MessageSize);
			}, RemotePort, ToStream);
	}
	/* 向指定流ToStream远程调用指定RemotePort上的函数，传入参数Arguments。当远程函数返回时，调用Callback，提供调用结果Result和返回值ReturnValue。
	如果远程端口未被监听，Callback将不会被调用。
	如果Result为Corrupted_object_received，说明收到了意外的返回值。这可能是因为远程函数返回值的类型与预期不符，或者有其它远程对象向本地端口发送了垃圾信息。此次远程调用将被废弃。
	如果Result为Parameter_message_incomplete，说明远程函数接收到的参数不完整。这可能是因为远程函数的参数类型与预期不符，或者有其它本地对象向远程端口发送了垃圾信息。此次远程调用将被废弃。
	ReturnType和ArgumentType必须是平凡类型。
	此方法保证Callback被调用时中断处于启用状态。
	*/
	template <typename CallbackType, typename... ArgumentType>
	inline void RemoteInvoke(uint8_t RemotePort, const CallbackType& Callback, Stream& ToStream, ArgumentType... Arguments)
	{
		RemoteInvoke(RemotePort, std::move_only_function<std::_FunctionSignature_t<CallbackType>>(Callback), ToStream, Arguments...);
	}
	/* 向Serial远程调用指定RemotePort上的函数，传入参数Arguments。当远程函数返回时，调用Callback，提供调用结果Result和返回值ReturnValue。
	如果远程端口未被监听，Callback将不会被调用。
	如果Result为Corrupted_object_received，说明收到了意外的返回值。这可能是因为远程函数返回值的类型与预期不符，或者有其它远程对象向本地端口发送了垃圾信息。此次远程调用将被废弃。
	如果Result为Parameter_message_incomplete，说明远程函数接收到的参数不完整。这可能是因为远程函数的参数类型与预期不符，或者有其它本地对象向远程端口发送了垃圾信息。此次远程调用将被废弃。
	ReturnType和ArgumentType必须是平凡类型。
	此方法保证Callback被调用时中断处于启用状态。
	*/
	template <typename ReturnType, typename... ArgumentType>
	inline void RemoteInvoke(uint8_t RemotePort, std::move_only_function<void(Exception Result, ReturnType ReturnValue) const>&& Callback, ArgumentType... Arguments)
	{
		RemoteInvoke(RemotePort, std::move(Callback), Serial, Arguments...);
	}
	/* 向Serial远程调用指定RemotePort上的函数，传入参数Arguments。当远程函数返回时，调用Callback，提供调用结果Result和返回值ReturnValue。
	如果远程端口未被监听，Callback将不会被调用。
	如果Result为Corrupted_object_received，说明收到了意外的返回值。这可能是因为远程函数返回值的类型与预期不符，或者有其它远程对象向本地端口发送了垃圾信息。此次远程调用将被废弃。
	如果Result为Parameter_message_incomplete，说明远程函数接收到的参数不完整。这可能是因为远程函数的参数类型与预期不符，或者有其它本地对象向远程端口发送了垃圾信息。此次远程调用将被废弃。
	ReturnType和ArgumentType必须是平凡类型。
	此方法保证Callback被调用时中断处于启用状态。
	*/
	template <typename CallbackType, typename... ArgumentType>
	inline void RemoteInvoke(uint8_t RemotePort, const CallbackType& Callback, ArgumentType... Arguments)
	{
		RemoteInvoke(RemotePort, std::move_only_function<std::_FunctionSignature_t<CallbackType>>(Callback), Serial, Arguments...);
	}
}