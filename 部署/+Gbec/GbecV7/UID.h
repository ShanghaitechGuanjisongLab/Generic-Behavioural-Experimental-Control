#pragma once
#include <stdint.h>
enum class UID : uint8_t
{
	//固定的内置函数端口
	Port_CreateProcess,
	Port_ThrowException,
	//异常代码
	Exception_UndecipherableMessage
};