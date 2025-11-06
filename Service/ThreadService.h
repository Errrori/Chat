#pragma once
#include "Data/IThreadRepository.h"

class ChatThread;
using RespCallback = std::function<void(const drogon::HttpResponsePtr&)>;


class ThreadService
{
	friend class Container;
public:
	ThreadService(const ThreadService&) = delete;
	ThreadService& operator=(const ThreadService&) = delete;
	ThreadService(ThreadService&& service) noexcept;
	ThreadService& operator=(ThreadService&& service) noexcept;
	ThreadService(std::shared_ptr<IThreadRepository> ptr):_repo(std::move(ptr)){}

	// create / join / query
	void CreateChatThread(const std::shared_ptr<ChatThread> chat, RespCallback&& callback) const;
	void QueryThreadInfo(int thread_id, RespCallback&& callback) const;
	void AddThreadMember(const MemberData& data, RespCallback&& callback, std::string operator_uid = "") const;
	std::vector<std::string> GetThreadMember(int thread_id) const noexcept(false);
	std::future<ChatThread::ThreadType> GetThreadType(int thread_id) const;


	bool ValidateMember(int thread_id, const std::string& uid);
private:  
	// a repository pointer
	std::shared_ptr<IThreadRepository> _repo;

};