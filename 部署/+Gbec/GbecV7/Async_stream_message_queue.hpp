#pragma once
#include <dynarray>
#include <functional>
namespace Async_stream_message_queue
{
	enum class Exception
	{
		Success,
		Port_occupied,
		Insufficient_message_capacity,
	};
	
	// 每个循环都应当调用此方法。它将实际处理所有排队中的操作，包括发送消息和调用监听器。其它所有方法都不实际执行任何操作，仅仅将该操作的描述符加入队列。
	void ExecuteQueuedOperations();

	// 排队向指定目标发送指定长度的消息，默认发往0号端口。消息将在下次ExecuteQueuedOperations时发送，调用方应确保那时Message指针仍然有效。
	void Send(const char *Message, size_t Length, Stream &ToStream, uint8_t ToPort);

	// 排队向指定目标发送指定长度的消息，默认发往0号端口。消息将在下次ExecuteQueuedOperations时发送。Message的所有权将被转移到内部，调用方不应再使用它。
	void Send(std::dynarray<char> &&Message, Stream &ToStream, uint8_t ToPort);

	/*监听FromStream流的FromPort端口。当远程传来指向FromPort的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback。在那之前，调用方有义务维持Message指针有效。如果这个监听可能被取消，调用方应保留返回的Handle，以便在不再需要监听时调用Close。
	监听是一次性的。一旦收到消息，监听就结束，Handle变成无效。不应对无效Handle调用Close，将导致未定义行为。
	如果FromPort正被其它Handle监听，那么这个监听将失败，返回Port_occupied，Handle无效。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity，Handle同样会变成无效。
	 */
	Exception Receive(char *Message, size_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream, void *&Handle, uint8_t FromPort);

	/*监听FromStream流的FromPort端口。当远程传来指向FromPort的消息时，调用Callback，提供消息内容Message。如果这个监听可能被取消，调用方应保留返回的Handle，以便在不再需要监听时调用Close。
	监听是一次性的。一旦收到消息，监听就结束，Handle变成无效。不应对无效Handle调用Close，将导致未定义行为。
	如果FromPort正被其它Handle监听，那么这个监听将失败，返回Port_occupied，Handle无效。
	 */
	Exception Receive(std::move_only_function<void(std::dynarray<char> Message) const> &&Callback, Stream &FromStream, void *&Handle, uint8_t FromPort);

	/*持续监听FromStream流的FromPort端口。每当远程传来指向FromPort的消息时，那个消息都将被拷贝到Message指针最多Capacity字节，并调用Callback。如果这个监听可能被停止，调用方应保留返回的Handle，以便在不再需要监听时调用Close。在停止监听前，调用方有义务维持Message指针有效。
	如果FromPort正被其它Handle监听，那么这个监听将失败，返回Port_occupied，Handle无效。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity，Handle仍然有效，监听仍然继续。
	 */
	Exception Listen(char *Message, size_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream, void *&Handle, uint8_t FromPort);

	/*持续监听FromStream流的FromPort端口。每当远程传来指向FromPort的消息时，调用Callback，并提供消息内容Message。如果这个监听可能被停止，调用方应保留返回的Handle，以便在不再需要监听时调用Close。
	如果FromPort正被其它Handle监听，那么这个监听将失败，返回Port_occupied，Handle无效。
	 */
	Exception Listen(std::move_only_function<void(std::dynarray<char> Message) const> &&Callback, Stream &FromStream, void *&Handle, uint8_t FromPort);

	/*自动分配一个空闲端口返回，并监听FromStream流的那个端口。当远程传来指向该端口的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback。在那之前，调用方有义务维持Message指针有效。如果这个监听可能被取消，调用方应保留返回的Handle，以便在不再需要监听时调用Close。
	监听是一次性的。一旦收到消息，监听就结束，Handle变成无效。不应对无效Handle调用Close，将导致未定义行为。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity，Handle同样会变成无效。
	 */
	uint8_t Receive(char *Message, size_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream, void *&Handle);

	/*自动分配一个空闲端口返回，并监听FromStream流的那个端口。当远程传来指向该端口的消息时，调用Callback，提供消息内容Message。如果这个监听可能被取消，调用方应保留返回的Handle，以便在不再需要监听时调用Close。
	监听是一次性的。一旦收到消息，监听就结束，Handle变成无效。不应对无效Handle调用Close，将导致未定义行为。
	 */
	uint8_t Receive(std::move_only_function<void(std::dynarray<char> Message) const> &&Callback, Stream &FromStream, void *&Handle);

	/*自动分配一个空闲端口返回，并持续监听FromStream流的那个端口。每当远程传来指向该端口的消息时，那个消息都将被拷贝到Message指针最多Capacity字节，并调用Callback。如果这个监听可能被停止，调用方应保留返回的Handle，以便在不再需要监听时调用Close。在停止监听前，调用方有义务维持Message指针有效。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity，Handle仍然有效，监听仍然继续。
	 */
	uint8_t Listen(char *Message, size_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream, void *&Handle);

	/*自动分配一个空闲端口返回，并持续监听FromStream流的那个端口。每当远程传来指向该端口的消息时，调用Callback，并提供消息内容Message。如果这个监听可能被停止，调用方应保留返回的Handle，以便在不再需要监听时调用Close。
	 */
	uint8_t Listen(std::move_only_function<void(std::dynarray<char> Message) const> &&Callback, Stream &FromStream, void *&Handle);

	// 取消返回此Handle的监听任务。Handle将变成无效。调用方有义务确保Handle是有效的，否则行为未定义。
	void Close(void *Handle);
}