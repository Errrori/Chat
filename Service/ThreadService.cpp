#include "pch.h"
#include "ThreadService.h"

#include "Common/ChatThread.h"


void ThreadService::CreateChatThread(const std::shared_ptr<ChatThread> chat, RespCallback&& callback)
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

void ThreadService::QueryThreadInfo(int thread_id, RespCallback&& callback)
{
	auto future = std::async(std::launch::async, [thread_id, this, callback = std::move(callback)]()
		{
			try
			{
				auto thread_info = _repo->GetThreadInfo(thread_id).get();
				if (thread_info==Json::nullValue)
				{
					LOG_ERROR << "can not find thread info";
					callback(Utils::CreateErrorResponse(400, 400, "can not find thread info"));
					return;
				}
				callback(Utils::CreateSuccessJsonResp(200, 200, "success get thread info", thread_info));
			}
			catch (const std::exception& e)
			{
				LOG_ERROR << "error: " << e.what();
				callback(Utils::CreateErrorResponse(400, 400, "can not find thread info"));
			}
		});

}


