#pragma once
#include "IThreadRepository.h"

class PrivateThread;
class GroupThread;
class AIThread;

class SQLiteThreadRepository:public IThreadRepository
{
public:
	std::future<int> CreatePrivateThread(const std::shared_ptr<PrivateThread> info) override;
	std::future<int> CreateGroupThread(const std::shared_ptr<GroupThread> info) override;
	std::future<int> CreateAIThread(const std::shared_ptr<AIThread> info) override;
	std::future<bool> AddToThread(const MemberData& member) override;

	std::future<Json::Value> GetThreadInfo(int thread_id) override;
	std::future<std::vector<std::string>> GetThreadMember(int thread_id) override ;
	std::future<ChatThread::ThreadType> GetThreadType(int thread_id) noexcept override;

	bool IsThreadMember(int thread_id, const std::string& uid) override;

};

