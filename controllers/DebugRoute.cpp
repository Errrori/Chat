#include "pch.h"
#include "DebugRoute.h"
#include "../manager/ConnectionManager.h"
#include "manager/ThreadManager.h"

void DbInfoController::GetPrivateChatInfo(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto& data = ThreadManager::GetPrivateChatInfo();
    callback(drogon::HttpResponse::newHttpJsonResponse(data));
}

void DbInfoController::GetThreadsInfo(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	const auto& data = ThreadManager::GetThreadInfo();
	callback(drogon::HttpResponse::newHttpJsonResponse(data));
}

void DbInfoController::ModifyUserInfo(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    Json::Value response;

    auto json_body = req->getJsonObject();
    if (!json_body) {
        response["code"] = 400;
        response["message"] = "request format error";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);
        return;
    }

	//判断是否包含必须字段
    if (!json_body->isMember("account")&&!json_body->isMember("uid"))
    {
        response["code"] = 400;
        response["message"] = "Invalid request data";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }


    auto json_data = Utils::UserInfo::FromJson(*json_body);
    LOG_INFO << json_data.ToString();
	bool result = DatabaseManager::ModifyUserInfo(json_data);


    if (!result)
    {
        response["code"] = 400;
		response["message"] = "fail to modify user's info ";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);
        return;
    }

	response["code"] = 200;
    response["message"] = "success to modify user's info ";
    response["data"] = *json_body;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
	callback(resp);
	
}

void DbInfoController::GetDbInfo(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    LOG_TRACE << "access info route\n";
    Json::Value response;

    try {
        auto users = DatabaseManager::GetAllUsersInfo();
        response["code"] = 200;
        response["message"] = "success to get database";
        response["data"] = users;
        response["total"] = users.size();
    }
    catch (const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "fail to get db_info: " << e.base().what();
        response["code"] = 500;
    }

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    if (response["code"].asUInt()!=200)
    {
        response["message"] = "get database error";
    }
    callback(resp);
}

void DbInfoController::ModifyName(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    LOG_TRACE << "Accessing modify username route\n";
    Json::Value response;
    response["code"] = 200;
    response["message"] = "Success";
    
    auto jsonBody = req->getJsonObject();
    if (!jsonBody) {
        response["code"] = 400;
        response["message"] = "Invalid request data";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    std::string uid = (*jsonBody)["uid"].asString();
    std::string name = (*jsonBody)["username"].asString();
    
    if (uid.empty() || name.empty()) {
        response["code"] = 400;
        response["message"] = "User ID or new username cannot be empty";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    bool success = DatabaseManager::ModifyUsername(uid, name);
    if (!success) {
        response["code"] = 404;
        response["message"] = "User does not exist or username not changed";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k404NotFound);
        callback(resp);
        return;
    }
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k200OK);
    callback(resp);
}

void DbInfoController::ModifyPassword(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    LOG_TRACE << "Accessing modify password route\n";
    Json::Value response;
    response["code"] = 200;
    response["message"] = "Success";
    
    auto jsonBody = req->getJsonObject();
    if (!jsonBody) {
        response["code"] = 400;
        response["message"] = "Invalid request data";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    std::string uid = (*jsonBody)["uid"].asString();
    std::string password = (*jsonBody)["password"].asString();
    
    if (uid.empty() || password.empty()) {
        response["code"] = 400;
        response["message"] = "User ID or new password cannot be empty";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    // Hash the password
    std::string hashedPassword = Utils::Authentication::PasswordHashed(password);
    bool success = DatabaseManager::ModifyPassword(uid, hashedPassword);
    if (!success) {
        response["code"] = 404;
        response["message"] = "User does not exist or password not changed";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k404NotFound);
        callback(resp);
        return;
    }
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k200OK);
    callback(resp);
}

void DbInfoController::DeleteUser(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    LOG_TRACE << "Accessing delete user route\n";
    Json::Value response;
    response["code"] = 200;
    response["message"] = "Success";
    
    auto jsonBody = req->getJsonObject();
    if (!jsonBody) {
        response["code"] = 400;
        response["message"] = "Invalid request data";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    std::string uid = (*jsonBody)["uid"].asString();
    
    if (uid.empty()) {
        response["code"] = 400;
        response["message"] = "User ID cannot be empty";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    bool success = DatabaseManager::DeleteUser(uid);
    if (!success) {
        response["code"] = 404;
        response["message"] = "User does not exist or deletion failed";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k404NotFound);
        callback(resp);
        return;
    }
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k200OK);
    callback(resp);
}


void DbInfoController::GetUser(const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    LOG_TRACE << "Accessing get user by id route\n";
    Json::Value response;
    response["code"] = 200;
    response["message"] = "Success";
    
    // Get user id from query parameter
    auto uid = req->getParameter("uid");
	auto account = req->getParameter("account");


    if (uid.empty() && account.empty()) {
        auto resp = Utils::CreateErrorResponse(400, 400, "User ID or account can not be empty");
        callback(resp);
        return;
	}

    if (!uid.empty()&&!account.empty())
    {
        auto resp = Utils::CreateErrorResponse(400, 400, "can not query user in two parameters");
        callback(resp);
        return;
    }

    Json::Value userInfo;
	bool success = false;
    
	if (!uid.empty())
	{
        success = DatabaseManager::GetUserInfoByUid(uid, userInfo);
	}
    else
    {
		success = DatabaseManager::GetUserQueryInfoByAccount(account, userInfo);
    }

    if (!success) {
        auto resp = Utils::CreateErrorResponse(404, 404, "User is not found");
        callback(resp);
        return;
    }
    
    response["data"] = userInfo;
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k200OK);
    callback(resp);
}

//void DbInfoController::ImportUsers(const drogon::HttpRequestPtr& req,
//    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
//{
//    LOG_TRACE << "access import_users\n";
//    Json::Value response;
//    response["code"] = 200;
//    response["message"] = "success import";
//    
//    auto jsonBody = req->getJsonObject();
//    if (!jsonBody || !(*jsonBody)["users"].isArray()) {
//        response["code"] = 400;
//        response["message"] = "invalid import，need to provide users vector";
//        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
//        resp->setStatusCode(drogon::k400BadRequest);
//        callback(resp);
//        return;
//    }
//    
//    const Json::Value& users = (*jsonBody)["users"];
//    Json::Value successUsers = Json::Value(Json::arrayValue);
//    Json::Value failedUsers = Json::Value(Json::arrayValue);
//    
//    for (int i = 0; i < users.size(); i++) {
//        const Json::Value& user = users[i];
//
//        if (!user.isMember("username") || !user["username"].isString() || user["username"].asString().empty() ||
//            !user.isMember("password") || !user["password"].isString() || user["password"].asString().empty()) {
//            
//            Json::Value failedUser;
//            failedUser["index"] = i;
//            failedUser["reason"] = "username or password is empty";
//            if (user.isMember("username")) failedUser["username"] = user["username"];
//            failedUsers.append(failedUser);
//            continue;
//        }
//        
//        std::string name = user["username"].asString();
//        std::string password = user["password"].asString();
//        std::string uid = user.isMember("uid") && user["uid"].isString() && !user["uid"].asString().empty() 
//            ? user["uid"].asString() : Utils::GenerateUid();
//
//        if (User::FindUserByUid(uid)) {
//            Json::Value failedUser;
//            failedUser["index"] = i;
//            failedUser["username"] = name;
//            failedUser["reason"] = "username already existed";
//            failedUsers.append(failedUser);
//            continue;
//        }
//
//        // 添加用户
//        bool success = User::AddUser(name, password, uid);
//        
//        if (success) {
//            Json::Value successUser;
//            successUser["username"] = name;
//            successUser["uid"] = uid;
//            successUsers.append(successUser);
//        } else {
//            Json::Value failedUser;
//            failedUser["index"] = i;
//            failedUser["username"] = name;
//            failedUser["reason"] = "fail to add user";
//            failedUsers.append(failedUser);
//        }
//    }
//    
//    response["data"]["success_count"] = static_cast<int>(successUsers.size());
//    response["data"]["failed_count"] = static_cast<int>(failedUsers.size());
//    response["data"]["success_users"] = successUsers;
//    response["data"]["failed_users"] = failedUsers;
//    
//    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
//    resp->setStatusCode(drogon::k200OK);
//    callback(resp);
//}

void DbInfoController::GetOnlineUsers(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    Json::Value response;
    response["code"] = 200;
    response["message"] = "Success getting online users";

    try {
        auto users = ConnectionManager::GetInstance().GetOnlineUsers();
        response["data"] = users;
        response["size"] = users.size();
    }
    catch (const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "fail to get name: " << e.base().what();
        auto resp = Utils::CreateErrorResponse(500, 500, "Failed to get online users: " + std::string(e.base().what()));
        callback(resp);
        return;
    }

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    callback(resp);
}

void DbInfoController::GetChatRecords(const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    LOG_INFO << "Get chat records accessed";
    Json::Value json_resp;
    Json::Value records;
    auto num = req->getOptionalParameter<unsigned int>("num");
    auto existing_id = req->getOptionalParameter<int64_t>("existing_id");
	auto thread_id = req->getOptionalParameter<unsigned int>("thread_id");

    if (existing_id.has_value()&&thread_id.has_value())
    {
        records = DatabaseManager::GetMessages(thread_id.value(),existing_id.value(),num.value_or(DataBase::DEFAULT_RECORDS_QUERY_LEN));
    }else
    {
        auto resp = Utils::CreateErrorResponse(400, 400, "Missing required field: existing_id");
        callback(resp);
        return;
    }

	if (records ==Json::nullValue)
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

void DbInfoController::GetAllRecords(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    LOG_INFO << "Get all records accessed";
    Json::Value json_resp;
    Json::Value records;
    auto num = req->getOptionalParameter<unsigned int>("num");
    auto thread_id = req->getOptionalParameter<unsigned int>("thread_id");

    if (num.has_value())
    {
        records = DatabaseManager::GetAllMessage(thread_id.value(), num.value());
    }
    else
    {
        records = DatabaseManager::GetAllMessage(thread_id.value());
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

void DbInfoController::GetAllNotifications(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    callback(drogon::HttpResponse::newHttpJsonResponse(DatabaseManager::GetNotifications()));
}

void DbInfoController::ModifyUserAvatar(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto& data = req->getJsonObject();
    std::string uid = (*data)["uid"].asString();
    std::string avatar = (*data)["avatar"].asString();
    bool is_success = DatabaseManager::ModifyAvatar(uid, avatar);
	Json::Value json_resp;
	if (is_success)
    {
        json_resp["code"] = 200;
        json_resp["message"] = "Operate success!";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(json_resp);
        callback(resp);
    }
    else
    {
        json_resp["code"] = 400;
        json_resp["message"] = "Operate fail!";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(json_resp);
        callback(resp);
    }
}

void DbInfoController::HandleDbInfoOptions(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    LOG_DEBUG << "DbInfoController::HandleDbInfoOptions called for /db_info";
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k204NoContent);
    callback(resp);
}

void DbInfoController::HandleGetFriendships(const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    callback(drogon::HttpResponse::newHttpJsonResponse(DatabaseManager::GetRelationships()));
}