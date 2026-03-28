#include "pch.h"
#include "Common/ResponseHelper.h"
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
            co_return ResponseHelper::MakeResponse(400, 400, "can not get request data");

        auto thread_info = PrivateThread::FromJson(*req_body);

        if (!thread_info.IsDataValid())
            co_return ResponseHelper::MakeResponse(400, 400, "lack of essential fields");

        auto thread_id = co_await Container::GetInstance().GetThreadService()->CreateChatThread(thread_info);

        Json::Value json_resp;
        json_resp["code"] = 200;
        json_resp["message"] = "success create private chat";
        json_resp["thread_id"] = thread_id;

        co_return drogon::HttpResponse::newHttpJsonResponse(json_resp);

    }catch (const std::exception& e)
    {
        LOG_ERROR << "exception: " << e.what();
        co_return ResponseHelper::MakeResponse(500, 500, "can not create thread");
    }


}

drogon::Task<drogon::HttpResponsePtr> ThreadController::CreateGroupThread(drogon::HttpRequestPtr req)
{
    try
    {
        const auto& req_body = req->getJsonObject();

        if (!req_body)
            co_return ResponseHelper::MakeResponse(400, 400, "can not get request data");

        const auto& uid = req->getAttributes()->get<std::string>("uid");
        auto thread_info = GroupThread::FromJson(*req_body);
        thread_info.SetOwnerUid(uid);

    	    LOG_INFO << "owner uid: " << uid;
        LOG_INFO << "thread info: " << thread_info.ToJson().toStyledString();

        if (!thread_info.IsDataValid())
            co_return ResponseHelper::MakeResponse(400, 400, "lack of essential fields");

        auto thread_id = co_await Container::GetInstance().GetThreadService()->CreateChatThread(thread_info);

        Json::Value json_resp;
        json_resp["code"] = 200;
        json_resp["message"] = "success create group chat";
        json_resp["thread_id"] = thread_id;

        co_return drogon::HttpResponse::newHttpJsonResponse(json_resp);
	    
    }catch (const std::exception& e)
    {
		LOG_ERROR << "exception: " << e.what();
        co_return ResponseHelper::MakeResponse(500,500,"system error");
    }
}

drogon::Task<drogon::HttpResponsePtr> ThreadController::CreateAIThread(drogon::HttpRequestPtr req)
{
    try
    {
        const auto& req_body = req->getJsonObject();

        if (!req_body)
            co_return ResponseHelper::MakeResponse(400, 400, "can not get request data");


        auto thread_info = AIThread::FromJson(*req_body);
        const auto& uid = req->getAttributes()->get<std::string>("uid");
        thread_info.SetCreatorUid(uid);

        if (!thread_info.IsDataValid())
            co_return ResponseHelper::MakeResponse(400, 400, "lack of essential fields");

        auto thread_id = co_await Container::GetInstance().GetThreadService()->CreateChatThread(thread_info);
        Json::Value json_resp;
        json_resp["code"] = 200;
        json_resp["message"] = "success create ai chat";
        json_resp["thread_id"] = thread_id;
		co_return drogon::HttpResponse::newHttpJsonResponse(json_resp);
    }catch (const std::exception& e)
    {
		LOG_ERROR << "exception: " << e.what();
		co_return ResponseHelper::MakeResponse(500, 500, "system error");
    }
    
}

drogon::Task<drogon::HttpResponsePtr> ThreadController::QueryThreadInfo(drogon::HttpRequestPtr req)
{
    try
    {
        auto thread_id_opt = req->getOptionalParameter<int>("thread_id");
        if (!thread_id_opt.has_value())
            co_return ResponseHelper::MakeResponse(400,400,"lack of essential field");
        
        auto info = co_await Container::GetInstance().GetThreadService()->QueryThreadInfo(thread_id_opt.value());

        co_return ResponseHelper::MakeResponse(200,200,"query successful",info);
    }
    catch (const std::exception& e)
    {
	    LOG_ERROR<<"exception: "<<e.what();
    	co_return ResponseHelper::MakeResponse(500, 500, "system error");
    }
}

drogon::Task<drogon::HttpResponsePtr> ThreadController::AddThreadMember(drogon::HttpRequestPtr req)
{
    try
    {
        const auto& req_body = req->getJsonObject();
        if (!req_body)
            co_return ResponseHelper::MakeResponse(400, 400, "can not get request data");

        const auto& json_data = *req_body;
        if (!json_data.isMember("thread_id") || !json_data.isMember("user_uid"))
            co_return ResponseHelper::MakeResponse(400, 400, "lack of essential fields");

        const auto& uid = req->getAttributes()->get<std::string>("uid");
        auto member_data = MemberData::FromJson(json_data);

		auto result = co_await Container::GetInstance().GetThreadService()->ValidateMemberCoro(member_data.GetThreadId(), uid);

        if (!result)
			co_return ResponseHelper::MakeResponse(400, 400, "not access to operate");

        auto is_success = co_await Container::GetInstance().GetThreadService()->AddThreadMember(member_data);

        //here need to check if the visitor is admin or owner of the group
        if (is_success)
			co_return ResponseHelper::MakeResponse(200, 200, "success add member");
        
    	co_return ResponseHelper::MakeResponse(400, 400, "can not add member");

    }catch (const std::exception& e)
    {
        LOG_ERROR << "exception: " << e.what();
        co_return ResponseHelper::MakeResponse(500, 500, "system error");
    }

}

drogon::Task<drogon::HttpResponsePtr> ThreadController::JoinThread(drogon::HttpRequestPtr req)
{
    try
    {
        const auto& req_body = req->getJsonObject();
        if (!req_body)
            co_return ResponseHelper::MakeResponse(400, 400, "can not get request data");
         
        const auto& json_data = *req_body;
        if (!json_data.isMember("thread_id"))
            co_return ResponseHelper::MakeResponse(400, 400, "lack of essential fields");

        const auto& uid = req->getAttributes()->get<std::string>("uid");

        auto member_data = MemberData::FromJson(json_data);
        member_data.SetUserUid(uid);

        auto result = co_await Container::GetInstance().GetThreadService()->AddThreadMember(member_data);
        if (result)
			co_return ResponseHelper::MakeResponse(200, 200, "success join thread");
		co_return ResponseHelper::MakeResponse(400, 400, "can not join thread");

    }catch (const std::exception& e)
    {
        LOG_ERROR << "exception: " << e.what();
        co_return ResponseHelper::MakeResponse(500, 500, "system error");
    }
   
}

drogon::Task<drogon::HttpResponsePtr> ThreadController::GetAIContext(drogon::HttpRequestPtr req)
{
    try
    {
        auto thread_id = req->getOptionalParameter<int>("thread_id");
        auto existed_time = req->getOptionalParameter<int64_t>("existed_time");
        const auto& uid = req->getAttributes()->get<std::string>("uid");

        if (!thread_id.has_value()||!existed_time.has_value())
            co_return ResponseHelper::MakeResponse(400, 400, "lack of essential fields");

        if (! co_await Container::GetInstance().GetThreadService()->ValidateMemberCoro(thread_id.value(), uid))
            co_return ResponseHelper::MakeResponse(400, 400, "not in thread");
    
		auto records = co_await Container::GetInstance().GetMessageService()->GetAIRecords(thread_id.value(),existed_time.value());

		co_return drogon::HttpResponse::newHttpJsonResponse(records);
    }catch (const std::exception& e)
    {
		LOG_ERROR << "exception in GetAIContext: " << e.what();
        co_return ResponseHelper::MakeResponse(400, 400, "exception to get records");
    }
}

drogon::Task<drogon::HttpResponsePtr> ThreadController::GetChatRecords(drogon::HttpRequestPtr req)
{
    try
    {
        auto thread_id = req->getOptionalParameter<int>("thread_id");
        auto existed_time = req->getOptionalParameter<int64_t>("existing_id");
		auto num = req->getOptionalParameter<int>("num");
        const auto& uid = req->getAttributes()->get<std::string>("uid");

        LOG_INFO << "GetChatRecords params: thread_id=" << (thread_id.has_value() ? thread_id.value() : -1)
                 << ", existing_id=" << (existed_time.has_value() ? existed_time.value() : -1)
                 << ", num=" << (num.has_value() ? num.value() : -1);

        if (!thread_id.has_value() || !existed_time.has_value())
           co_return ResponseHelper::MakeResponse(400, 400, "lack of essential fields");
         

        if (num.has_value() && num.value() <= 0)
            co_return ResponseHelper::MakeResponse(400, 400, "invalid parameter value");

        if (!co_await Container::GetInstance().GetThreadService()->ValidateMemberCoro(thread_id.value(), uid))
        {
            LOG_ERROR << "user is not in thread: " << thread_id.value();
            co_return ResponseHelper::MakeResponse(400, 400, "not in thread");
        }

    	auto records = co_await Container::GetInstance().GetMessageService()->GetChatRecords(thread_id.value(), num.value_or(50),existed_time.value());

        co_return drogon::HttpResponse::newHttpJsonResponse(records);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "exception in GetChatRecords: " << e.what();
        co_return ResponseHelper::MakeResponse(400, 400, "exception to get records");
    }
}

drogon::Task<drogon::HttpResponsePtr> ThreadController::GetChatOverviews(drogon::HttpRequestPtr req)
{
    try
    {
        auto existing_id = req->getOptionalParameter<int64_t>("existing_id");

        if (!existing_id.has_value())
        {
            co_return ResponseHelper::MakeResponse(400, 400, "Missing field");
        }

        const auto& uid = req->getAttributes()->get<std::string>("uid");

		auto overview = co_await Container::GetInstance().GetMessageService()->GetChatOverviews(existing_id.value(), uid);

        if (overview == Json::nullValue)
        {
            co_return ResponseHelper::MakeResponse(400, 400, "can not get record");
        }

        co_return drogon::HttpResponse::newHttpJsonResponse(overview);
    }
    catch (const std::exception& e)
    {
        co_return ResponseHelper::MakeResponse(500, 500, "Server error");
    }
}

drogon::Task<drogon::HttpResponsePtr> ThreadController::GetUserChatRecords(drogon::HttpRequestPtr req)
{
    try
    {
        auto thread_id = req->getOptionalParameter<int>("thread_id");
        auto existed_id = req->getOptionalParameter<int64_t>("existing_id");
        auto num = req->getOptionalParameter<int>("num");
        const auto& uid = req->getAttributes()->get<std::string>("uid");

        if (!thread_id.has_value() || !existed_id.has_value())
            co_return ResponseHelper::MakeResponse(400, 400, "lack of essential fields");
        if ((num.has_value() && num.value() <= 0)||
            (existed_id.has_value()&&existed_id < 0))
            co_return ResponseHelper::MakeResponse(400, 400, "invalid argument");

        if (!co_await Container::GetInstance().GetThreadService()->ValidateMemberCoro(thread_id.value(), uid))
            co_return ResponseHelper::MakeResponse(400, 400, "not in thread");

        auto records = co_await Container::GetInstance().GetMessageService()
    		->GetChatRecords(thread_id.value(),num.value_or(50),existed_id.value_or(0));

        if (records == Json::nullValue)
            LOG_ERROR << "can not find records";

        co_return drogon::HttpResponse::newHttpJsonResponse(records);

    }catch (const std::exception& e)
    {
        LOG_ERROR << "exception: " << e.what();
        co_return ResponseHelper::MakeResponse(500, 500, e.what());
    }
    catch (const drogon::orm::DrogonDbException& e)
    {
        LOG_ERROR << "exception: " << e.base().what();
        co_return ResponseHelper::MakeResponse(500, 500, e.base().what());
    }

}
