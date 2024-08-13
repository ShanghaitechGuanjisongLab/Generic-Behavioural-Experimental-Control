#pragma once
#include <Cpp_Standard_Library.h>
#include <dynarray>
void SendMessage(uint8_t To, std::dynarray<char> Message);
struct LocalObject
{
	LocalObject();
	//必须实现远程消息处理方法
	virtual void RemoteCall(std::dynarray<char> Message) = 0;
};