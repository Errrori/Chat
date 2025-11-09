#pragma once
#include "IThreadRepository.h"

class PrivateThread;
class GroupThread;
class AIThread;

class SQLiteThreadRepository:public IThreadRepository
{
public:
	drogon::Task<int> CreatePrivateThread(PrivateThread info) override;
	drogon::Task<int> CreateGroupThread(GroupThread info) override;
	drogon::Task<int> CreateAIThread(AIThread info) override;
	drogon::Task<bool> AddToGroup(const MemberData& member) override;

	drogon::Task<Json::Value> GetThreadInfo(int thread_id) override;
	drogon::Task<std::vector<std::string>> GetThreadMember(int thread_id) override ;
	drogon::Task<ChatThread::ThreadType> GetThreadType(int thread_id) override;

	drogon::Task<bool> IsThreadMember(int thread_id, const std::string& uid) override;

};

