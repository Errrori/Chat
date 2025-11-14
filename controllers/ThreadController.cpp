#include "pch.h"
#include "ThreadController.h"
#include "Common/User.h"
#include "Container.h"
#include "Common/ChatThread.h"
#include "Service/ConnectionService.h"
#include "Service/MessageService.h"
#include "Service/ThreadService.h"

drogon::Task<drogon::HttpResponsePtr> ThreadController::CreatePrivateThread(drogon::HttpRequestPtr req)
{
    try
    {
        const auto& req_body = req->getJsonObject();

        if (!req_body)
            co_return Utils::CreateErrorResponse(400, 400, "can not get request data");

        auto thread_info = PrivateThread::FromJson(*req_body);

        if (!thread_info.IsDataValid())
            co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");

        auto thread_id = co_await GET_THREAD_SERVICE->CreateChatThread(thread_info);

        Json::Value json_resp;
        json_resp["code"] = 200;
        json_resp["message"] = "success create private chat";
        json_resp["thread_id"] = thread_id;

        co_return drogon::HttpResponse::newHttpJsonResponse(json_resp);

    }catch (const std::exception& e)
    {
        LOG_ERROR << "exception: " << e.what();
        co_return Utils::CreateErrorResponse(500, 500, "can not create thread");
    }


}

drogon::Task<drogon::HttpResponsePtr> ThreadController::CreateGroupThread(drogon::HttpRequestPtr req)
{
    try
    {
        const auto& req_body = req->getJsonObject();

        if (!req_body)
            co_return Utils::CreateErrorResponse(400, 400, "can not get request data");

        const auto& user_info = req->getAttributes()->get<UserInfo>("visitor_info");
        auto thread_info = GroupThread::FromJson(*req_body);
        thread_info.SetOwnerUid(user_info.getUid());

        LOG_INFO << "owner uid: " << user_info.getUid();
        LOG_INFO << "thread info: " << thread_info.ToJson().toStyledString();

        if (!thread_info.IsDataValid())
            co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");

        auto thread_id = co_await GET_THREAD_SERVICE->CreateChatThread(thread_info);

        Json::Value json_resp;
        json_resp["code"] = 200;
        json_resp["message"] = "success create group chat";
        json_resp["thread_id"] = thread_id;

        co_return drogon::HttpResponse::newHttpJsonResponse(json_resp);
	    
    }catch (const std::exception& e)
    {
		LOG_ERROR << "exception: " << e.what();
        co_return Utils::CreateErrorResponse(500,500,"system error");
    }
}

drogon::Task<drogon::HttpResponsePtr> ThreadController::CreateAIThread(drogon::HttpRequestPtr req)
{
    try
    {
        const auto& req_body = req->getJsonObject();

        if (!req_body)
            co_return Utils::CreateErrorResponse(400, 400, "can not get request data");


        auto thread_info = AIThread::FromJson(*req_body);
        const auto& user_info = req->getAttributes()->get<UserInfo>("visitor_info");
        thread_info.SetCreatorUid(user_info.getUid());

        if (!thread_info.IsDataValid())
            co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");

        auto thread_id = co_await GET_THREAD_SERVICE->CreateChatThread(thread_info);
        Json::Value json_resp;
        json_resp["code"] = 200;
        json_resp["message"] = "success create ai chat";
        json_resp["thread_id"] = thread_id;
		co_return drogon::HttpResponse::newHttpJsonResponse(json_resp);
    }catch (const std::exception& e)
    {
		LOG_ERROR << "exception: " << e.what();
		co_return Utils::CreateErrorResponse(500, 500, "system error");
    }
    
}

drogon::Task<drogon::HttpResponsePtr> ThreadController::QueryThreadInfo(drogon::HttpRequestPtr req)
{
    try
    {
        auto thread_id_opt = req->getOptionalParameter<int>("thread_id");
        if (!thread_id_opt.has_value())
            co_return Utils::CreateErrorResponse(400,400,"lack of essential field");
        
        auto info = co_await GET_THREAD_SERVICE->QueryThreadInfo(thread_id_opt.value());

        co_return Utils::CreateSuccessJsonResp(200,200,"query successful",info);
    }
    catch (const std::exception& e)
    {
	    LOG_ERROR<<"exception: "<<e.what();
    	co_return Utils::CreateErrorResponse(500, 500, "system error");
    }
}

drogon::Task<drogon::HttpResponsePtr> ThreadController::AddThreadMember(drogon::HttpRequestPtr req)
{
    try
    {
        const auto& req_body = req->getJsonObject();
        if (!req_body)
            co_return Utils::CreateErrorResponse(400, 400, "can not get request data");

        const auto& json_data = *req_body;
        if (!json_data.isMember("thread_id") || !json_data.isMember("user_uid"))
            co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");

        const auto& visitor_info = req->getAttributes()->get<UserInfo>("visitor_info");
        auto member_data = MemberData::FromJson(json_data);

		auto result = co_await GET_THREAD_SERVICE->ValidateMemberCoro(member_data.GetThreadId(), visitor_info.getUid());

        if (!result)
			co_return Utils::CreateErrorResponse(400, 400, "not access to operate");

        auto is_success = co_await GET_THREAD_SERVICE->AddThreadMember(member_data);

        //here need to check if the visitor is admin or owner of the group
        if (is_success)
			co_return Utils::CreateSuccessResp(200, 200, "success add member");
        
    	co_return Utils::CreateErrorResponse(400, 400, "can not add member");

    }catch (const std::exception& e)
    {
        LOG_ERROR << "exception: " << e.what();
        co_return Utils::CreateErrorResponse(500, 500, "system error");
    }

}

drogon::Task<drogon::HttpResponsePtr> ThreadController::JoinThread(drogon::HttpRequestPtr req)
{
    try
    {
        const auto& req_body = req->getJsonObject();
        if (!req_body)
            co_return Utils::CreateErrorResponse(400, 400, "can not get request data");
         
        const auto& json_data = *req_body;
        if (!json_data.isMember("thread_id"))
            co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");

        const auto& user_info = req->getAttributes()->get<UserInfo>("visitor_info");

        auto result = co_await GET_THREAD_SERVICE->AddThreadMember(MemberData::FromJson(json_data));
        if (result)
			co_return Utils::CreateSuccessResp(200, 200, "success join thread");
		co_return Utils::CreateErrorResponse(400, 400, "can not join thread");

    }catch (const std::exception& e)
    {
        LOG_ERROR << "exception: " << e.what();
        co_return Utils::CreateErrorResponse(500, 500, "system error");
    }
   
}

drogon::Task<drogon::HttpResponsePtr> ThreadController::GetAIContext(drogon::HttpRequestPtr req)
{
    try
    {
        auto thread_id = req->getOptionalParameter<int>("thread_id");
        auto existed_time = req->getOptionalParameter<int64_t>("existed_time");
        const auto& user_info = req->getAttributes()->get<UserInfo>("visitor_info");

        if (!thread_id.has_value()||!existed_time.has_value())
            co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");

        if (! co_await GET_THREAD_SERVICE->ValidateMemberCoro(thread_id.value(), user_info.getUid()))
            co_return Utils::CreateErrorResponse(400, 400, "not in thread");
    
        const auto& records = co_await GET_MESSAGE_SERVICE->GetAIRecords(thread_id.value(),existed_time.value());

		co_return drogon::HttpResponse::newHttpJsonResponse(records);
    }catch (const std::exception& e)
    {
		LOG_ERROR << "exception in GetAIContext: " << e.what();
        co_return Utils::CreateErrorResponse(400, 400, "exception to get records");
    }
}

drogon::Task<drogon::HttpResponsePtr> ThreadController::GetChatRecords(drogon::HttpRequestPtr req)
{
    try
    {
        auto thread_id = req->getOptionalParameter<int>("thread_id");
        auto existed_time = req->getOptionalParameter<int64_t>("existing_id");
		auto num = req->getOptionalParameter<int>("num");
        const auto& user_info = req->getAttributes()->get<UserInfo>("visitor_info");

        if (!thread_id.has_value() || !existed_time.has_value())
           co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");
         

        if (num.has_value() && num.value() <= 0)
            co_return Utils::CreateErrorResponse(400, 400, "invalid parameter value");

        if (!co_await GET_THREAD_SERVICE->ValidateMemberCoro(thread_id.value(), user_info.getUid()))
        {
            LOG_ERROR << "user is not in thread: " << thread_id.value();
            co_return Utils::CreateErrorResponse(400, 400, "not in thread");
        }

    	auto records = co_await GET_MESSAGE_SERVICE->GetChatRecords(thread_id.value(), num.value_or(50),existed_time.value());

        co_return drogon::HttpResponse::newHttpJsonResponse(records);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "exception in GetAIContext: " << e.what();
        co_return Utils::CreateErrorResponse(400, 400, "exception to get records");
    }
}

drogon::Task<drogon::HttpResponsePtr> ThreadController::GetChatOverviews(drogon::HttpRequestPtr req)
{
    try
    {
        auto existing_id = req->getOptionalParameter<int64_t>("existing_id");

        if (!existing_id.has_value())
        {
            co_return Utils::CreateErrorResponse(400, 400, "Missing field");
        }

        const auto& info = req->getAttributes()->get<UserInfo>("visitor_info");

        auto overview = co_await GET_MESSAGE_SERVICE->GetChatOverviews(existing_id.value(), info.getUid());

        if (overview == Json::nullValue)
        {
            co_return Utils::CreateErrorResponse(400, 400, "can not get record");
        }

        co_return drogon::HttpResponse::newHttpJsonResponse(overview);
    }
    catch (const std::exception& e)
    {
        co_return Utils::CreateErrorResponse(500, 500, "Server error");
    }
}
