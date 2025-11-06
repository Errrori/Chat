#pragma once

#include "Common/ChatThread.h"
#include "Common/Convert.h"
class PrivateThread;
class GroupThread;
class AIThread;

class MemberData;

using GetMemberCallback = std::function<void(const std::vector<std::string>&)>;
using GetThreadTypeCallback = std::function<void(ChatThread::ThreadType)>;

class IThreadRepository
{
public:
	virtual std::future<int> CreatePrivateThread(const std::shared_ptr<PrivateThread> info) = 0;
	virtual std::future<int> CreateGroupThread(const std::shared_ptr<GroupThread> info) = 0;
	virtual std::future<int> CreateAIThread(const std::shared_ptr<AIThread> info) = 0;

	virtual std::future<Json::Value> GetThreadInfo(int thread_id) = 0;
	virtual std::future<bool> AddToThread(const MemberData& member) = 0;
	virtual bool IsThreadMember(int thread_id, const std::string& uid) = 0;
	virtual std::future<std::vector<std::string>> GetThreadMember(int thread_id) = 0;
	virtual std::future<ChatThread::ThreadType> GetThreadType(int thread_id) = 0;

	virtual ~IThreadRepository() = default;
};

