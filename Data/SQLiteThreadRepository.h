#pragma once
#include "IThreadRepository.h"
#include <drogon/orm/DbClient.h>

class PrivateThread;
class GroupThread;
class AIThread;

class SQLiteThreadRepository:public IThreadRepository
{
public:
	explicit SQLiteThreadRepository(drogon::orm::DbClientPtr db) : _db(std::move(db)) {}
	drogon::Task<int> CreatePrivateThread(PrivateThread info) override;
	drogon::Task<int> CreateGroupThread(GroupThread info) override;
	drogon::Task<int> CreateAIThread(AIThread info) override;
	drogon::Task<bool> AddToGroup(const MemberData& member) override;

	drogon::Task<Json::Value> GetThreadInfo(int thread_id) override;
	drogon::Task<std::vector<std::string>> GetThreadMember(int thread_id) override ;
	drogon::Task<ChatThread::ThreadType> GetThreadType(int thread_id) override;

	drogon::Task<bool> IsThreadMember(int thread_id, const std::string& uid) override;
	drogon::Task<std::pair<ChatThread::ThreadType, std::vector<std::string>>> GetTypeAndMembers(int thread_id) override;
	drogon::Task<std::pair<ChatThread::ThreadType, std::vector<std::string>>> GetMembersAndType(int thread_id) override;

private:
	drogon::orm::DbClientPtr _db;
};

