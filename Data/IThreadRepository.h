#pragma once

#include <drogon/drogon.h>
#include <json/json.h>
#include <vector>
#include <functional>
#include <string>

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
    virtual drogon::Task<int> CreatePrivateThread(PrivateThread info) = 0;
    virtual drogon::Task<int> CreateGroupThread(GroupThread info) = 0;
    virtual drogon::Task<int> CreateAIThread(AIThread info) = 0;

	//	virtual drogon::Task<int> CreatePrivateThread(const std::shared_ptr<PrivateThread> info) = 0;
	//  virtual drogon::Task<int> CreateGroupThread(const std::shared_ptr<GroupThread> info) = 0;
	//  virtual drogon::Task<int> CreateAIThread(const std::shared_ptr<AIThread> info) = 0;

	virtual drogon::Task<Json::Value> GetThreadInfo(int thread_id) = 0;
	virtual drogon::Task<bool> AddToGroup(const MemberData& member) = 0;
	virtual drogon::Task<bool> IsThreadMember(int thread_id, const std::string& uid) = 0;
	virtual drogon::Task<std::vector<std::string>> GetThreadMember(int thread_id) = 0;
	virtual drogon::Task<ChatThread::ThreadType> GetThreadType(int thread_id) = 0;

    virtual ~IThreadRepository() = default;
};

