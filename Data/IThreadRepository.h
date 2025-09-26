#pragma once

class PrivateThread;
class GroupThread;
class AIThread;
class ChatThread;

class IThreadRepository
{
public:
	virtual std::future<int> CreatePrivateThread(const std::shared_ptr<PrivateThread> info) = 0;
	virtual std::future<int> CreateGroupThread(const std::shared_ptr<GroupThread> info) = 0;
	virtual std::future<int> CreateAIThread(const std::shared_ptr<AIThread> info) = 0;

	virtual std::future<Json::Value> GetThreadInfo(int thread_id) = 0;
	virtual std::future<bool> JoinThread() = 0;

	~IThreadRepository() = default;
};

