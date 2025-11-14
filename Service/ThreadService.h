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
	ThreadService(std::shared_ptr<IThreadRepository> ptr):_thread_repo(std::move(ptr)){}

	// create / join / query
	drogon::Task<int> CreateChatThread(const ChatThread& chat) const;
	drogon::Task<Json::Value> QueryThreadInfo(int thread_id) const;
	drogon::Task<bool> AddThreadMember(const MemberData& data) const;
	drogon::Task<std::vector<std::string>> GetThreadMember(int thread_id) const;
	drogon::Task<ChatThread::ThreadType> GetThreadType(int thread_id) const;

	drogon::Task<bool> ValidateMemberCoro(int thread_id, const std::string& uid) const;

	bool ValidateMember(int thread_id, const std::string& uid) const;


private:  
	// a repository pointer
	std::shared_ptr<IThreadRepository> _thread_repo;

};