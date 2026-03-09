#include "pch.h"
#include "ThreadService.h"

#include "Common/ChatThread.h"

ThreadService::ThreadService(ThreadService&& service) noexcept
{
	this->_thread_repo = std::move(service._thread_repo);
	service._thread_repo = nullptr;
}

ThreadService& ThreadService::operator=(ThreadService&& service) noexcept
{
	if (this!=&service)
	{
		this->_thread_repo = std::move(service._thread_repo);
		service._thread_repo = nullptr;
	}
	return *this;
}

drogon::Task<int> ThreadService::CreateChatThread(const ChatThread& chat) const
{
	try
	{
		switch (chat.GetThreadType())
		{
		case ChatThread::ThreadType::PRIVATE:
			co_return co_await _thread_repo->CreatePrivateThread(dynamic_cast<const PrivateThread&>(chat));
		case ChatThread::ThreadType::GROUP:
			co_return co_await _thread_repo->CreateGroupThread(dynamic_cast<const GroupThread&>(chat));
		case ChatThread::ThreadType::AI:
			co_return co_await _thread_repo->CreateAIThread(dynamic_cast<const AIThread&>(chat));
		default:
			LOG_ERROR << "unknown thread type";
			throw std::invalid_argument("unknown thread type");
		}
	}catch (const std::exception& e)
	{
		throw;
	}
}

drogon::Task<Json::Value> ThreadService::QueryThreadInfo(int thread_id) const
{
	try
	{
		const auto info = co_await _thread_repo->GetThreadInfo(thread_id);
		if (info == Json::nullValue)
			co_return Json::nullValue;
		
		auto members = co_await _thread_repo->GetThreadMember(thread_id);
		Json::Value thread_info;
		thread_info["info"] = info;
		Json::Value members_info(Json::arrayValue);
		for (const auto& member : members)
		{
			members_info.append(member);
		}
		thread_info["members"] = members_info;
		co_return thread_info;
	}
	catch (const std::exception& e)
	{
		throw;
	}
}

//if operator_uid is not empty, check if the operator is a member of the thread
drogon::Task<bool> ThreadService::AddThreadMember(const MemberData& data) const
{
	try
	{
		if (co_await _thread_repo->AddToGroup(data))
			co_return true;
		else
			co_return false;
	}
	catch (const std::exception& e)
	{
		throw;
	}
}


drogon::Task<std::vector<std::string>> ThreadService::GetThreadMember(int thread_id) const
{
	try
	{
		if (thread_id <= 0)
		{
			LOG_ERROR << "invalid thread_id";
			throw std::invalid_argument("invalid thread_id: "+std::to_string(thread_id));
		}
		auto result = co_await _thread_repo->GetThreadMember(thread_id);
		co_return result;
	}catch (const std::exception& e){
		LOG_ERROR << "exception in GetThreadMember: " << e.what();
		co_return {};
	}
}

drogon::Task<ChatThread::ThreadType> ThreadService::GetThreadType(int thread_id) const
{
	try
	{
		co_return co_await _thread_repo->GetThreadType(thread_id);
	}catch (const std::exception& e)
	{
		throw;
	}
}

drogon::Task<bool> ThreadService::ValidateMemberCoro(int thread_id, const std::string& uid) const
{
	try
	{
		auto members = co_await GetThreadMember(thread_id);

		if (std::find(members.begin(), members.end(), uid) != members.end())
			co_return true;

		co_return false;
	}catch (const std::exception& e)
	{
		throw;
	}
	
}

drogon::Task<std::pair<ChatThread::ThreadType, std::vector<std::string>>>
ThreadService::GetMembersAndType(int thread_id) const
{
	try
	{
		co_return co_await _thread_repo->GetMembersAndType(thread_id);
	}
	catch (const std::exception& e)
	{
		throw;
	}
}




