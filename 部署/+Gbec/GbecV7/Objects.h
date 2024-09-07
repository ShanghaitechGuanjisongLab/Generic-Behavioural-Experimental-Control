#pragma once
#include "UID.h"
#include "Async_stream_IO.hpp"
#include <unordered_map>
#include <random>
struct Object
{
	virtual UID Start() = 0;
	virtual UID Repeat(uint16_t Times) { return UID::Exception_MethodNotSupported; }
	virtual UID Pause() { return UID::Exception_MethodNotSupported; }
	virtual UID Continue() { return UID::Exception_MethodNotSupported; }
	virtual UID Abort() { return UID::Exception_MethodNotSupported; }
	virtual void GetInformation(std::vector<char> &) = 0;
	virtual ~Object();
};
extern const std::unordered_map<UID, Object *(*)()> ObjectCreators;
struct Placeholder : Object
{
	UID Pause() override
	{
		return UID::Exception_ObjectFinished;
	}
	UID Continue() override
	{
		return UID::Exception_ObjectFinished;
	}
	UID Abort() override
	{
		return UID::Exception_ObjectFinished;
	}
	void GetInformation(std::vector<char> &Buffer) override
	{
		static constexpr UID Information[] PROGMEM = {UID::Property_ObjectType, UID::Object_Placeholder};
		Buffer.resize(Buffer.size() + sizeof(Information));
		memcpy_P(Buffer.data() + Buffer.size() - sizeof(Information), Information, sizeof(Information));
	}
};
template <typename ObjectType>
inline Object *New()
{
	return new ObjectType;
}
#define RegisterObject(ObjectType) {UID::Object_##ObjectType, New<ObjectType>}