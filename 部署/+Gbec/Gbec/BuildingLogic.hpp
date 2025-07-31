#pragma once
#include "UID.hpp"
#include "Async_stream_IO.hpp"
#include <unordered_map>
#include <vector>
#include <map>
#include <sstream>
extern Async_stream_IO::AsyncStream SerialStream;

struct Node
{
	UID const ID;
	constexpr Node(UID ID) : ID(ID) {}
	virtual void Enter(std::unordered_map<UID, uint16_t> &TrialsDone) = 0;
	virtual void SubmitInfo(std::map<UID, std::string> &Info){}
};

// 此节点不实际执行回合，仅仅检查本回合是否应当跳过；如果应当，在TrialsDone中将自己的计数-1然后进入SkipTo；否则，进入Content
struct TrialNode : Node
{
	Node *SkipTo;
	constexpr TrialNode(UID ID)
		: Node(ID) {}
	TrialNode(UID ID, Node *Content)
		: Node(ID) {}
	void Enter(std::unordered_map<UID, uint16_t> &TrialsDone) override
	{
		auto const it = TrialsDone.find(ID);
		if (it == TrialsDone.end())
		{
			Content->Enter(TrialsDone);
			return;
		}
		if (it->second == 0)
		{
			TrialsDone.erase(it);
			Content->Enter(TrialsDone);
			return;
		}
		it->second--;
		if (it->second == 0)
			TrialsDone.erase(it);
		SkipTo->Enter(TrialsDone);
	}
	void SubmitInfo(std::map<UID, std::string> &Info) override
	{
		if (!Info.contains(ID))
		{
			std::ostringstream Collector;
			Collector<<
		}
	}
};