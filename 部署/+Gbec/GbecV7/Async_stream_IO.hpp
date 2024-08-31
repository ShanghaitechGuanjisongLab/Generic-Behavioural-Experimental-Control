#pragma once
#include <Cpp_Standard_Library.h>
#include <dynarray>
#include <functional>
#include <Arduino.h>
namespace Async_stream_IO
{
	enum class Exception
	{
		Success,
		Port_occupied,
		Insufficient_message_capacity,
		Port_idle,
	};

	/*一般应在loop中调用此方法。它将实际执行所有排队中的事务，包括发送消息和调用监听器。此方法执行过程中会允许中断，即使调用前处于不可中断状态。此方法返回之前会恢复调用前的中断状态。
	不应在此方法执行期间存在任何可能会直接调用Stream读写操作的中断，可能破坏数据结构。只能使用此命名空间中的方法进行委托。
	*/
	void ExecuteTransactionsInQueue();

	/* 委托下次ExecuteTransactionsInQueue时，向指定目标发送指定长度的消息。调用方应确保那时Message指针仍然有效。消息发送后，调用Callback通知委托完成。
	写串口操作依赖中断，在不允许中断的状态下使用Stream原生方法写出可能会永不返回，因此需要调用此方法委托延迟写出。反之则可以直接调用Stream提供的写出方法，无需委托。
	如果在Callback中再次调用Send要非常小心，因为那个委托会直接在本次ExecuteTransactionsInQueue中执行而不等到下一次，然后再次调用Callback，如果那个Callback中又有Send，这样就存在形成无限循环的风险。
	*/
	void Send(const void *Message, uint8_t Length, uint8_t ToPort, std::move_only_function<void() const> &&Callback, Stream &ToStream = Serial);

	/* 委托下次ExecuteTransactionsInQueue时，向指定目标发送指定长度的消息。调用方应确保那时Message指针仍然有效。
	写串口操作依赖中断，在不允许中断的状态下使用Stream原生方法写出可能会永不返回，因此需要调用此方法委托延迟写出。反之则可以直接调用Stream提供的写出方法，无需委托。
	*/
	void Send(const void *Message, uint8_t Length, uint8_t ToPort, Stream &ToStream = Serial);

	/* 委托下次ExecuteTransactionsInQueue时，向指定目标发送消息。Message的所有权将被转移到内部，调用方不应再使用它。消息发送后，可选调用Callback通知委托完成。
	写串口操作依赖中断，在不允许中断的状态下使用Stream原生方法写出可能会永不返回，因此需要调用此方法委托延迟写出。反之则可以直接调用Stream提供的写出方法，无需委托。
	如果在Callback中再次调用Send要非常小心，因为那个委托会直接在本次ExecuteTransactionsInQueue中执行而不等到下一次，然后再次调用Callback，如果那个Callback中又有Send，这样就存在形成无限循环的风险。
	*/
	void Send(std::dynarray<char> &&Message, uint8_t ToPort, std::move_only_function<void() const> &&Callback, Stream &ToStream = Serial);

	/* 委托下次ExecuteTransactionsInQueue时，向指定目标发送消息。Message的所有权将被转移到内部，调用方不应再使用它。
	写串口操作依赖中断，在不允许中断的状态下使用Stream原生方法写出可能会永不返回，因此需要调用此方法委托延迟写出。反之则可以直接调用Stream提供的写出方法，无需委托。
	*/
	void Send(std::dynarray<char> &&Message, uint8_t ToPort, Stream &ToStream = Serial);

	/*监听FromStream流的FromPort端口。当远程传来指向FromPort的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback。在那之前，调用方有义务维持Message指针有效。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	此监听是一次性的。一旦收到消息，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity。
	 */
	Exception Receive(void *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, uint8_t FromPort, Stream &FromStream = Serial);

	/*监听FromStream流的FromPort端口。当远程传来指向FromPort的消息时，调用Callback，提供消息内容Message。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	此监听是一次性的。一旦收到消息，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	 */
	Exception Receive(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, uint8_t FromPort, Stream &FromStream = Serial);

	/*持续监听FromStream流的FromPort端口。每当远程传来指向FromPort的消息时，那个消息都将被拷贝到Message指针最多Capacity字节，并调用Callback。在停止监听前，调用方有义务维持Message指针有效。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity，监听仍然继续。
	 */
	Exception Listen(void *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, uint8_t FromPort, Stream &FromStream = Serial);

	/*持续监听FromStream流的FromPort端口。每当远程传来指向FromPort的消息时，调用Callback，并提供消息内容Message。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	 */
	Exception Listen(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, uint8_t FromPort, Stream &FromStream = Serial);

	/*自动分配一个空闲端口返回，并监听FromStream流的那个端口。当远程传来指向该端口的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback。在那之前，调用方有义务维持Message指针有效。
	此监听是一次性的。一旦收到消息，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity。
	 */
	uint8_t Receive(void *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream = Serial);

	/*自动分配一个空闲端口返回，并监听FromStream流的那个端口。当远程传来指向该端口的消息时，调用Callback，提供消息内容Message。
	此监听是一次性的。一旦收到消息，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	 */
	uint8_t Receive(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream = Serial);

	/*自动分配一个空闲端口返回，并持续监听FromStream流的那个端口。每当远程传来指向该端口的消息时，那个消息都将被拷贝到Message指针最多Capacity字节，并调用Callback。在停止监听前，调用方有义务维持Message指针有效。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity，监听仍然继续。
	 */
	uint8_t Listen(void *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream = Serial);

	// 自动分配一个空闲端口返回，并持续监听FromStream流的那个端口。每当远程传来指向该端口的消息时，调用Callback，并提供消息内容Message。
	uint8_t Listen(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream = Serial);

	// 释放指定端口，取消任何Receive或Listen监听。如果端口未被监听，返回Port_idle。无论如何，此方法返回后即可以在被释放的端口上附加新的监听。
	Exception ReleasePort(uint8_t Port);

	template <typename T, size_t Plus>
	struct _PlusAndPrepend;
	template <size_t Plus, size_t... Values>
	struct _PlusAndPrepend<std::index_sequence<Values...>, Plus>
	{
		using type = std::index_sequence<Plus, Plus + Values...>;
	};

	template <typename T>
	struct _CumSum;
	template <size_t First, size_t... Rest>
	struct _CumSum<std::index_sequence<First, Rest...>>
	{
		using type = _PlusAndPrepend<First, _CumSum<std::index_sequence<Rest...>>::type>::type;
	};
	template <size_t Only>
	struct _CumSum<std::index_sequence<Only>>
	{
		using type = std::index_sequence<Only>;
	};

	template <typename Offsets>
	struct _InvokeWithMemoryOffsets;
	template <size_t... Offsets>
	struct _InvokeWithMemoryOffsets<std::index_sequence<Offsets...>>
	{
		template <typename ReturnType, typename... ArgumentType>
		static ReturnType Invoke(std::move_only_function<ReturnType(ArgumentType...)> Function, const char *Memory)
		{
			return Function(*reinterpret_cast<const ArgumentType *>(Memory + Offsets)...);
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

	// 主模板，同时也是针对可调用对象（包括lambda）的特化
	template <typename T>
	struct _FunctionSignature : _FunctionSignature<decltype(&T::operator())>
	{
	};
	// 针对函数指针的特化
	template <typename ReturnType, typename... Args>
	struct _FunctionSignature<ReturnType (*)(Args...)>
	{
		using type = ReturnType(Args...);
	};
	// 针对成员函数指针的特化
	template <typename ReturnType, typename ClassType, typename... Args>
	struct _FunctionSignature<ReturnType (ClassType::*)(Args...)>
	{
		using type = ReturnType(Args...);
	};
	// 针对const成员函数指针的特化
	template <typename ReturnType, typename ClassType, typename... Args>
	struct _FunctionSignature<ReturnType (ClassType::*)(Args...) const>
	{
		using type = ReturnType(Args...);
	};

	// 将任意可调用对象绑定到指定端口上，当收到消息时调用。远程要调用此对象，需要将所有参数序列化拼接称一个连续的内存块，并且头部加上一个uint8_t的端口号用来接收返回值，然后将此消息发送到此函数绑定的端口。
	template <typename ReturnType, typename... ArgumentType>
	inline void BindFunctionToPort(std::move_only_function<ReturnType(ArgumentType...)> &&Function, uint8_t Port, Stream &ToStream = Serial)
	{
		constexpr size_t Capacity = _TypesSize<ArgumentType...>() + sizeof(uint8_t);
		static char Memory[Capacity];
		Listen(Memory, Capacity, [Function = std::move(Function), Memory, &ToStream](Exception Result)
			   {
			if (Result == Exception::Success)
			{
				// Memory的第一个字段是返回端口
				const ReturnType ReturnValue = _InvokeWithMemoryOffsets<_CumSum<std::index_sequence<sizeof(ArgumentType)...>>::type>::Invoke(std::move(Function), Memory + sizeof(uint8_t));
				Send(&ReturnValue, sizeof(ReturnValue), *reinterpret_cast<uint8_t *>(Memory), ToStream);
			} });
	}
	template <typename T>
	inline void BindFunctionToPort(const T &Function, uint8_t Port, Stream &ToStream = Serial)
	{
		BindFunctionToPort(std::move_only_function<typename _FunctionSignature<T>::type>(Function), Port, ToStream);
	}
}