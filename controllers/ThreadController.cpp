#include "pch.h"
#include "ThreadController.h"

#include "Container.h"
#include "Common/ChatThread.h"
#include "Service/MessageService.h"
#include "Service/ThreadService.h"

void ThreadController::CreatePrivateThread(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto& req_body = req->getJsonObject();

    if (!req_body)
    {
        callback(Utils::CreateErrorResponse(400, 400, "can not get request data"));
        return;
    }

    auto thread_info = PrivateThread::FromJson(*req_body);

    if (!thread_info->IsDataValid())
    {
        callback(Utils::CreateErrorResponse(400,400,"lack of essential fields"));
    }

    Container::GetInstance().GetService<ThreadService>()->CreateChatThread(thread_info, std::move(callback));
}

void ThreadController::CreateGroupThread(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try
    {
        const auto& req_body = req->getJsonObject();

        if (!req_body)
        {
            callback(Utils::CreateErrorResponse(400, 400, "can not get request data"));
            return;
        }

        const auto& user_info = req->getAttributes()->get<Utils::UsersInfo>("visitor_info");
        auto thread_info = GroupThread::FromJson(*req_body);
        thread_info->SetOwnerUid(user_info.uid);

        LOG_INFO << "thread info: " << thread_info->ToJson().toStyledString();

        if (!thread_info->IsDataValid())
        {
            callback(Utils::CreateErrorResponse(400, 400, "lack of essential fields"));
            return;
        }

        Container::GetInstance().GetService<ThreadService>()->CreateChatThread(thread_info, std::move(callback));
	    
    }catch (const std::exception& e)
    {
		LOG_ERROR << "exception: " << e.what();
        callback(Utils::CreateErrorResponse(500,500,"system error"));
    }
    

}

void ThreadController::CreateAIThread(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try
    {
        const auto& req_body = req->getJsonObject();

        if (!req_body)
        {
            callback(Utils::CreateErrorResponse(400, 400, "can not get request data"));
            return;
        }

        auto thread_info = AIThread::FromJson(*req_body);
        const auto& user_info = req->getAttributes()->get<Utils::UsersInfo>("visitor_info");
        thread_info->SetCreatorUid(user_info.uid);

        if (!thread_info->IsDataValid())
        {
            callback(Utils::CreateErrorResponse(400, 400, "lack of essential fields"));
            return;
        }

        Container::GetInstance().GetService<ThreadService>()->CreateChatThread(thread_info, std::move(callback));
    }catch (const std::exception& e)
    {
		LOG_ERROR << "exception: " << e.what();
		callback(Utils::CreateErrorResponse(500, 500, "system error"));
    }
    
}

void ThreadController::QueryThreadInfo(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    auto thread_id_opt = req->getOptionalParameter<int>("thread_id");
    if (!thread_id_opt.has_value())
    {
        callback(Utils::CreateErrorResponse(400, 400, "lack of essential fields"));
        return;
    }

	Container::GetInstance().GetService<ThreadService>()
		->QueryThreadInfo(thread_id_opt.value(), std::move(callback));
}

void ThreadController::AddThreadMember(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto& req_body = req->getJsonObject();
    if (!req_body)
    {
        callback(Utils::CreateErrorResponse(400, 400, "can not get request data"));
        return;
    }

    const auto& json_data = *req_body;
    if (!json_data.isMember("thread_id") || !json_data.isMember("user_uid"))
    {
        callback(Utils::CreateErrorResponse(400, 400, "lack of essential fields"));
        return;
    }
	const auto& visitor_info = req->getAttributes()->get<Utils::UsersInfo>("visitor_info");

	LOG_INFO << "visitor uid: " << visitor_info.uid;

    const auto& member_data = MemberData::FromJson(json_data);

	//here need to check if the visitor is admin or owner of the group
    Container::GetInstance().GetService<ThreadService>()
		->AddThreadMember(member_data,std::move(callback), visitor_info.uid);
}

void ThreadController::JoinThread(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto& req_body = req->getJsonObject();
    if (!req_body)
    {
        callback(Utils::CreateErrorResponse(400, 400, "can not get request data"));
        return;
    }
    const auto& json_data = *req_body;
    if (!json_data.isMember("thread_id") || !json_data.isMember("uid"))
    {
        callback(Utils::CreateErrorResponse(400, 400, "lack of essential fields"));
        return;
    }

	const auto& user_info = req->getAttributes()->get<Utils::UsersInfo>("visitor_info");
    
    if (user_info.uid!=json_data["uid"].asString())
    {
		callback(Utils::CreateErrorResponse(400, 400, "can not operate other users!"));
        return;
    }

    Container::GetInstance().GetService<ThreadService>()
		->AddThreadMember(MemberData::FromJson(json_data), std::move(callback));

}

void ThreadController::GetAIContext(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try
    {
        auto thread_id = req->getOptionalParameter<int>("thread_id");
        auto existed_time = req->getOptionalParameter<int64_t>("existed_time");
        const auto& user_info = req->getAttributes()->get<Utils::UsersInfo>("visitor_info");

        if (!thread_id.has_value()||!existed_time.has_value())
        {
            LOG_ERROR << "lack of field: thread_id";
			callback(Utils::CreateErrorResponse(400, 400, "lack of essential fields"));
        	return;
        }

        if (!GET_THREAD_SERVICE->ValidateMember(thread_id.value(), user_info.uid))
        {
            LOG_ERROR << "user is not in thread: "<<thread_id.value();
            callback(Utils::CreateErrorResponse(400, 400, "not in thread"));
            return;
        }

        const auto& records = GET_MESSAGE_SERVICE->GetAIRecords(thread_id.value(),existed_time.value());

		callback(drogon::HttpResponse::newHttpJsonResponse(records));

    }catch (const std::exception& e)
    {
        callback(Utils::CreateErrorResponse(400, 400, "exception to get records"));
		LOG_ERROR << "exception in GetAIContext: " << e.what();
    }
}

void ThreadController::GetChatRecords(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try
    {
        auto thread_id = req->getOptionalParameter<int>("thread_id");
        auto existed_time = req->getOptionalParameter<int64_t>("existing_id");
		auto num = req->getOptionalParameter<int>("num");
        const auto& user_info = req->getAttributes()->get<Utils::UsersInfo>("visitor_info");

        if (!thread_id.has_value() || !existed_time.has_value())
        {
            LOG_ERROR << "lack of field: thread_id";
            callback(Utils::CreateErrorResponse(400, 400, "lack of essential fields"));
            return;
        }

        if (num.has_value()&&num.value()<=0)
        {
            callback(Utils::CreateErrorResponse(400,400,"invalid parameter value"));
            return;
        }

        if (!GET_THREAD_SERVICE->ValidateMember(thread_id.value(), user_info.uid))
        {
            LOG_ERROR << "user is not in thread: " << thread_id.value();
            callback(Utils::CreateErrorResponse(400, 400, "not in thread"));
            return;
        }

        const auto& records = GET_MESSAGE_SERVICE->GetChatRecords(thread_id.value(), num.value_or(50),existed_time.value());

        callback(drogon::HttpResponse::newHttpJsonResponse(records));

    }
    catch (const std::exception& e)
    {
        callback(Utils::CreateErrorResponse(400, 400, "exception to get records"));
        LOG_ERROR << "exception in GetAIContext: " << e.what();
    }

}
