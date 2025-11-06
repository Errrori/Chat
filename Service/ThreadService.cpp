#include "pch.h"
#include "ThreadService.h"

#include "Common/ChatThread.h"


ThreadService::ThreadService(ThreadService&& service) noexcept
{
	this->_repo = std::move(service._repo);
	service._repo = nullptr;
}

ThreadService& ThreadService::operator=(ThreadService&& service) noexcept
{
	if (this!=&service)
	{
		this->_repo = std::move(service._repo);
		service._repo = nullptr;
	}
	return *this;
}

void ThreadService::CreateChatThread(const std::shared_ptr<ChatThread> chat, RespCallback&& callback) const
{
	auto result = std::async(std::launch::async, [chat,this,callback = std::move(callback)]()
		{
			std::future<int> future;
			switch (chat->GetThreadType())
			{
			case ChatThread::ThreadType::PRIVATE:
				future = _repo->CreatePrivateThread(std::dynamic_pointer_cast<PrivateThread>(chat));
				break;
			case ChatThread::ThreadType::GROUP:
				future = _repo->CreateGroupThread(std::dynamic_pointer_cast<GroupThread>(chat));
				break;
			case ChatThread::ThreadType::AI:
				future = _repo->CreateAIThread(std::dynamic_pointer_cast<AIThread>(chat));
				break;
			default:
				LOG_ERROR << "unknown thread type";
				callback(Utils::CreateErrorResponse(400, 400, "unknown thread type"));
				return;
			}

			auto thread_id = future.get();
			if (thread_id>0)
			{
				Json::Value resp;
				resp["thread_id"] = thread_id;
				callback(Utils::CreateSuccessJsonResp(200, 200, "success create thread", resp));
			}
			else
			{
				callback(Utils::CreateErrorResponse(400,400,"can not create thread"));
			}
		});
}

void ThreadService::QueryThreadInfo(int thread_id, RespCallback&& callback) const
{
	auto future = std::async(std::launch::async, [thread_id, this, callback = std::move(callback)]()
		{
			try
			{
				const auto& info = _repo->GetThreadInfo(thread_id).get();
				if (info==Json::nullValue)
				{
					LOG_ERROR << "can not find thread info";
					callback(Utils::CreateErrorResponse(400, 400, "can not find thread info"));
					return;
				}
				const auto& members = _repo->GetThreadMember(thread_id).get();
				Json::Value thread_info;
				thread_info["info"] = info;
				Json::Value members_info(Json::arrayValue);
				for (const auto& member:members)
				{
					members_info.append(member);
				}
				thread_info["members"] = members_info;

				callback(Utils::CreateSuccessJsonResp(200, 200, "success get thread info", thread_info));
			}
			catch (const std::exception& e)
			{
				LOG_ERROR << "error: " << e.what();
				callback(Utils::CreateErrorResponse(400, 400, "can not find thread info"));
			}
		});

}

//if operator_uid is not empty, check if the operator is a member of the thread
void ThreadService::AddThreadMember(const MemberData& data, RespCallback&& callback, std::string operator_uid) const
{
	auto future = std::async(std::launch::async, [data, this,callback = std::move(callback),&operator_uid]()
		{
			try
			{
				if (!operator_uid.empty())
				{
					bool is_member = _repo->IsThreadMember(data.GetThreadId(), operator_uid);
					if (!is_member)
					{
						callback(Utils::CreateErrorResponse(400, 400, "not access to operate"));
						return;
					}
				}
				else
				{
					callback(Utils::CreateErrorResponse(400, 400, "lack of operator info"));
				}

				if (_repo->AddToThread(data).get())
					callback(Utils::CreateSuccessResp(200, 200, "success add to thread"));
				else
					callback(Utils::CreateErrorResponse(400, 400, "fail to add to thread"));

			}
			catch (const std::exception& e)
			{
				LOG_ERROR << "error: " << e.what();
				callback(Utils::CreateErrorResponse(500, 500, "system error"));
			}
		});
}

std::vector<std::string> ThreadService::GetThreadMember(int thread_id) const noexcept(false)
{
	//���Կ�������ʹ��switch���
	try
	{
		auto result = _repo->GetThreadMember(thread_id);

		return result.get();
	}catch (const std::exception& e){
		LOG_ERROR << "exception in GetThreadMember: " << e.what();
		return {};
	}


}

std::future<ChatThread::ThreadType> ThreadService::GetThreadType(int thread_id) const
{
	return _repo->GetThreadType(thread_id);
}

bool ThreadService::ValidateMember(int thread_id, const std::string& uid)
{
	return true;
}

