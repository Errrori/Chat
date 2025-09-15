#include "pch.h"
#include "ThreadController.h"
#include "manager/ThreadManager.h"

void ThreadController::AddToThread(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto& json_body = req->getJsonObject();
    if(!json_body){
        LOG_ERROR << "can not get the data of request";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
        resp->setBody("can not get json body");
        callback(resp);
        return;
    }
    
    if(!json_body->isMember("thread_id")&&!json_body->isMember("uid")){
        LOG_ERROR << "invalid json body";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
        resp->setBody("invalid json body");
        callback(resp);
    }
    
    const auto& json_data = *json_body;
	const auto& thread_id = json_data["thread_id"].asInt();
	const auto& user_id = json_data["uid"].asString();
    
    const auto& visitor_info = req->getAttributes()->get<Utils::UserInfo>("visitor_info");
	if(!ThreadManager::ValidateMember(visitor_info.uid,thread_id)){
        callback(Utils::CreateErrorResponse(400, 400, "invitor is not in the thread!"));
        return;
    }

    if (ThreadManager::ValidateMember(user_id, thread_id))
    {
        callback(Utils::CreateErrorResponse(400, 400, "user already in the thread"));
        return;
    }
    
    bool result = ThreadManager::AddNewGroupMember(thread_id, user_id);

    if (!result)
    {
        callback(Utils::CreateErrorResponse(400,400,"error to invite user"));
        return;
    }

	Json::Value json_resp;
    json_resp["code"] = 200;
    json_resp["message"] = "success invite user, uid: "+user_id;
    callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));
	// If you want to build a GroupMemberData from json_data, you can uncomment the following line:
	//auto member = GroupMemberData::BuildFromJson(json_data);
}

void ThreadController::JoinThread(const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto& json_body = req->getJsonObject();
    if (!json_body) {
        LOG_ERROR << "can not get the data of request";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::HttpStatusCode::k400BadRequest);
        resp->setBody("can not get json body");
        callback(resp);
        return;
    }

    if (!json_body->isMember("thread_id")) {
        LOG_ERROR << "invalid json body";
        callback(Utils::CreateErrorResponse(400,400,"lack of essential field"));
        return;
    }

	const auto& json_data = *json_body;
    const auto& visitor_info = req->getAttributes()->get<Utils::UserInfo>("visitor_info");
	const auto& uid = visitor_info.uid;
    const auto& thread_id = json_data["thread_id"].asInt();

	if (ThreadManager::ValidateMember(uid, thread_id))
	{
		callback(Utils::CreateErrorResponse(400, 400, "user already in the thread"));
        return;
	}

    bool result = ThreadManager::AddNewGroupMember(thread_id, uid);
    if (!result) {
        callback(Utils::CreateErrorResponse(400, 400, "fail to join thread"));
        return;
    }
    Json::Value json_resp;
    json_resp["code"] = 200;
    json_resp["message"] = "success join thread,uid: "+visitor_info.uid;
	callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));
}

void ThreadController::GetThreadInfo(const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto& thread_id = req->getOptionalParameter<int>("thread_id");
    if (!thread_id.has_value())
    {
		callback(Utils::CreateErrorResponse(400, 400, "request error!"));
		return;
    }

    const auto& info = ThreadManager::GetGroupInfo(thread_id.value());

    if(!info.has_value())
    {        
        callback(Utils::CreateErrorResponse(400,400,"thread is not found"));
        return;
    }
    
 	Json::Value json_resp;
    json_resp["code"] = 200;
    json_resp["message"] = "success";
    json_resp["data"] = info.value();
    json_resp["size"] = info.value().size();
    callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));
}

void ThreadController::CreateGroupChats(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	//group_name, group_avatar, group_description

    const auto& json_body = req->getJsonObject();
    if (!json_body) {
        LOG_ERROR << "can not get the data of request";
        callback(Utils::CreateErrorResponse(400, 400, "error request format"));
        return;
    }

    const auto& json_data = *json_body;

    const auto& optional_data = GroupChatData::BuildFromJson(json_data);
    if(!optional_data.has_value()){
        callback(Utils::CreateErrorResponse(400, 400, "missing field!"));
        return;
    }

    const auto& group_data = optional_data.value();
    auto thread_id = ThreadManager::CreateGroupChat(group_data);
    if (!thread_id.has_value()) {
        callback(Utils::CreateErrorResponse(400, 400, "fail to create group chat"));
        return;
    }

	const auto& visitor_info = req->getAttributes()->get<Utils::UserInfo>("visitor_info");
    bool result = ThreadManager::AddNewGroupMember(thread_id.value(), visitor_info.uid,MASTER_ROLE);
    if (!result) {
        callback(Utils::CreateErrorResponse(400, 400, "fail to create thread"));
        return;
    }

    Json::Value json_resp;
    json_resp["thread_id"] = thread_id.value();
    json_resp["code"] = 200;
    json_resp["message"] = "success create group chat";
    callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));

}

void ThreadController::CreateAIChats(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto& json_body = req->getJsonObject();
    if (!json_body) {
        LOG_ERROR << "can not get the data of request";
        callback(Utils::CreateErrorResponse(400, 400, "error request format"));
        return;
    }

    const auto& json_data = *json_body;
    if (!json_data.isMember("name"))
    {
        LOG_ERROR << "ai need a name!";
		callback(Utils::CreateErrorResponse(400, 400, "lack of essential field"));
        return;
    }

    std::optional<int> thread_id;
    const auto& visitor_info = req->getAttributes()->get<Utils::UserInfo>("visitor_info");

    bool is_init = json_data.isMember("init_description");
    if (is_init)
    {
        thread_id = ThreadManager::CreateAIThread(json_data["name"].asString(), json_data["init_description"].asString(),visitor_info.uid );
    }
    else
    {
		thread_id = ThreadManager::CreateAIThread(json_data["name"].asString(), "", visitor_info.uid);
    }

    if (!thread_id.has_value())
    {
        LOG_ERROR << "can not create ai chat!";
        callback(Utils::CreateErrorResponse(400, 400, "can not create ai chat"));
        return;
    }

	Json::Value json_resp;
    json_resp["code"] = 200;
	json_resp["message"] = "success create ai chat";
	json_resp["thread_id"] = thread_id.value();
    callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));
}


void ThreadController::CreatePrivateChats(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto& json_body = req->getJsonObject();
    if (!json_body) {
        LOG_ERROR << "can not get the data of request";
        callback(Utils::CreateErrorResponse(400, 400, "error request format"));
        return;
    }

	const auto& json_data = *json_body;

    if (!json_data.isMember("first_uid")||!json_data.isMember("second_uid"))
    {
		LOG_ERROR << "lack of essential field";
        callback(Utils::CreateErrorResponse(400, 400, "lack of essential field"));
		return;
    }

	const auto& first_uid = json_data["first_uid"].asString();
	const auto& second_uid = json_data["second_uid"].asString();

    if (first_uid==second_uid)
    {
	    callback(Utils::CreateErrorResponse(400,400,"can not chat with yourself!"));
		return;
    }

    const auto& visitor_info = req->getAttributes()->get<Utils::UserInfo>("visitor_info");
    if (visitor_info.uid != first_uid&&visitor_info.uid!=second_uid)
    {
        callback(Utils::CreateErrorResponse(400,400,"can not operate other users!"));
        return;
    }

    auto thread_id = ThreadManager::CreatePrivateThread(first_uid, second_uid);

    if (!thread_id.has_value())
    {
        callback(Utils::CreateErrorResponse(400,400,"can not create chat"));
        return;
    }

	Json::Value data;
	data["thread_id"] = thread_id.value();
    Json::Value json_resp;
	json_resp["data"] = data;
    json_resp["code"] = 200;
	json_resp["message"] = "success create private chat";

	callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));
}

void ThreadController::GetUserThreadIds(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto& visitor_info = req->getAttributes()->get<Utils::UserInfo>("visitor_info");

    Json::Value thread_ids = ThreadManager::GetUserThreadIds(visitor_info.uid);

    Json::Value json_resp;
    json_resp["code"] = 200;
    json_resp["message"] = "success get user thread ids";
    json_resp["data"] = thread_ids;

    callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));
}

void ThreadController::GetChatRecords(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    LOG_INFO << "Get chat records accessed";
    Json::Value json_resp;
    Json::Value records;
    auto num = req->getOptionalParameter<unsigned int>("num");
    auto existing_id = req->getOptionalParameter<int64_t>("existing_id");
    auto thread_id = req->getOptionalParameter<unsigned int>("thread_id");

    if (existing_id.has_value() && thread_id.has_value())
    {
        records = DatabaseManager::GetMessages(thread_id.value(), existing_id.value(), num.value_or(DataBase::DEFAULT_RECORDS_QUERY_LEN));
    }
    else
    {
        auto resp = Utils::CreateErrorResponse(400, 400, "Missing required field: existing_id");
        callback(resp);
        return;
    }

    if (records == Json::nullValue)
    {
        auto resp = Utils::CreateErrorResponse(404, 404, "No records found in database");
        callback(resp);
        return;
    }

    json_resp["data"] = records;
    json_resp["size"] = records.empty() ? 0 : records.size();
    json_resp["code"] = 200;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json_resp);
    callback(resp);
}

void ThreadController::GetOverviewChat(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try
    {
        auto existing_id = req->getOptionalParameter<int64_t>("existing_id");

        if (!existing_id.has_value())
        {
            callback(Utils::CreateErrorResponse(400, 400, "Missing field"));
            return;
        }

        const auto& info = req->getAttributes()->get<Utils::UserInfo>("visitor_info");

       const auto& overview = ThreadManager::GetOverviewRecord(existing_id.value(), info.uid);

       if (!overview.has_value())
       {
		   callback(Utils::CreateErrorResponse(400, 400, "can not get record"));
           return;
       }

	   callback(drogon::HttpResponse::newHttpJsonResponse(overview.value_or(Json::nullValue)));

    }
	catch (const std::exception& e)
    {
		callback(Utils::CreateErrorResponse(500, 500, "Server error"));
    }
    

}
