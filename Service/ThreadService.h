#pragma once
#include "Data/IThreadRepository.h"

class ChatThread;
using RespCallback = std::function<void(const drogon::HttpResponsePtr&)>;

class ThreadService
{
	friend class Container;
public:
	ThreadService(std::shared_ptr<IThreadRepository> ptr):_repo(std::move(ptr)){}
	// create / join / query
	void CreateChatThread(const std::shared_ptr<ChatThread> chat, RespCallback&& callback);
	void QueryThreadInfo(int thread_id, RespCallback&& callback);
	void AddThreadMember();
	void GetThreadInfo(int thread_id,RespCallback&& callback);

private:  // 改为 protected
	// a repository pointer
	std::shared_ptr<IThreadRepository> _repo;

};