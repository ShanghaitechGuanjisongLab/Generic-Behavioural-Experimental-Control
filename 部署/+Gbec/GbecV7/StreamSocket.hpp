#pragma once
#include <functional>
#include <dynarray>
namespace StreamSocket
{
	enum class Exception
	{
		Success,
		Port_occupied,
		Port_bound,
		Port_unbound,
		Buffer_overflow,
		Stream_timeout
	};
	struct UdpSocket
	{
		// 使用指定的基础流构造对象。调用方负责设置流的超时时间、波特率等参数。所有Receive重载都可能返回Stream_timeout，表示流读取超时。
		UdpSocket(Stream &Base);
		// 绑定到指定端口。如果端口被占用，返回Port_occupied。此函数返回Success后，再次调用将返回Port_bound。所有Stream共享一套端口编号系统。如果没有Bind就调用Receive，将立即返回Port_unbound。
		Exception Bind(size_t Port) const;
		~UdpSocket();
		// 注册异步接收消息，收到消息时执行Handler函数。Handler函数接收两个参数，第一个是消息内容，第二个是消息来源端口。
		Exception Receive(std::move_only_function<void(std::dynarray<char> Buffer, size_t From) const> &&Handler) const;
		// 使用指定的Buffer，注册异步接收消息，收到消息时执行Handler函数，Handler函数接受一个参数，表示发送方端口号。
		Exception Receive(std::vector<char> &Buffer, std::move_only_function<void(size_t From) const> &&Handler);
		// 使用指定的Buffer和Capacity，注册异步接收消息。收到消息时执行Handler函数，Handler函数接受两个参数，第一个是消息长度，第二个是发送方端口号。如果NumBytes>Capacity，说明消息长度超过缓冲区容量，未能容纳的字节将被丢弃。
		Exception Receive(char *Buffer, size_t Capacity, std::move_only_function<void(size_t NumBytes, size_t From) const> &&Handler);
		// 向指定端口To发送NumBytes字节数的消息。如果没有绑定端口，默认发送端口号为0，表示匿名消息。
		void Send(const char *Buffer, size_t NumBytes, size_t To) const;
	};
	struct TcpServer
	{
		// 使用指定的基础流构造对象。调用方负责设置流的超时时间、波特率等参数。所有Receive重载都可能返回Stream_timeout，表示流读取超时。
		TcpSocket(Stream&Base);
		// 绑定到指定端口。如果端口被占用，返回Port_occupied。此函数返回Success后，再次调用将返回Port_bound。所有Stream共享一套端口编号系统。如果没有Bind就调用Receive，将立即返回Port_unbound。
		Exception Bind(size_t Port) const;
	};
	// 实际处理所有异步注册的Receive。调用方应该在主循环中调用此函数。
	void Loop();
}