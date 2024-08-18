#pragma once
#include <Cpp_Standard_Library.h>
#include <dynarray>
#include <Arduino.h>
//向远程端口发送消息
void SendMessage(uint8_t To, std::dynarray<char>&& Message);
//开始监听远程端口
uint8_t StartListen(std::move_only_function<void(uint8_t Size, Stream &Message) const> &&Listener);
//停止监听远程端口
void StopListen(uint8_t Port);