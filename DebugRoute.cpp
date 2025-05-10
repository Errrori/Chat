#include "DebugRoute.h"
#include "Users.h"
#include "Utils.h"
#include "ConnectionManager.h"

void DbInfoController::GetDbInfo(const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    std::cout << "access info route\n";
    auto db = DatabaseManager::GetDbClient();
    Json::Value response;
    response["code"] = 200;
    response["message"] = "success to get database";

    std::string sql = "SELECT * FROM users LIMIT 100;";

    try {
        auto result = db->execSqlSync(sql);

        Json::Value users = Json::Value(Json::arrayValue);
        for (const auto& row : result)
        {
            Json::Value user;
            user["id"] = row["id"].as<int>();
            user["username"] = row["username"].as<std::string>();
            user["uid"] = row["uid"].as<std::string>();
            user["create_time"] = row["create_time"].as<std::string>();

            users.append(user);
        }

        response["data"] = users;
        response["total"] = users.size();
    }
    catch (const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "fail to get name: " << e.base().what();
        response["code"] = 500;
    }

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    if (response["code"].asUInt()!=200)
    {
        resp->setStatusCode(drogon::k500InternalServerError);
    }
    else
    {
        resp->setStatusCode(drogon::k200OK);
    }
    callback(resp);
}

void DbInfoController::ModifyName(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    std::cout << "Accessing modify username route\n";
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
    
    bool success = Users::ModifyUserName(uid, name);
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
    std::cout << "Accessing modify password route\n";
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
    std::string hashedPassword = Utils::PasswordHashed(password);
    
    bool success = Users::ModifyUserPassword(uid, hashedPassword);
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
    std::cout << "Accessing delete user route\n";
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
    
    bool success = Users::DeleteUser(uid);
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


void DbInfoController::GetUserById(const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    std::cout << "Accessing get user by id route\n";
    Json::Value response;
    response["code"] = 200;
    response["message"] = "Success";
    
    // Get user id from query parameter
    auto uid = req->getParameter("uid");
    
    if (uid.empty()) {
        response["code"] = 400;
        response["message"] = "User ID cannot be empty";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    Json::Value userInfo;
    bool success = Users::GetUserInfoByUid(uid, userInfo);
    
    if (!success) {
        response["code"] = 404;
        response["message"] = "User not found";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k404NotFound);
        callback(resp);
        return;
    }
    
    response["data"] = userInfo;
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k200OK);
    callback(resp);
}

void DbInfoController::ImportUsers(const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    std::cout << "access import_users\n";
    Json::Value response;
    response["code"] = 200;
    response["message"] = "success import";
    
    auto jsonBody = req->getJsonObject();
    if (!jsonBody || !(*jsonBody)["users"].isArray()) {
        response["code"] = 400;
        response["message"] = "invalid import，need to provide users vector";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }
    
    const Json::Value& users = (*jsonBody)["users"];
    Json::Value successUsers = Json::Value(Json::arrayValue);
    Json::Value failedUsers = Json::Value(Json::arrayValue);
    
    for (int i = 0; i < users.size(); i++) {
        const Json::Value& user = users[i];

        if (!user.isMember("username") || !user["username"].isString() || user["username"].asString().empty() ||
            !user.isMember("password") || !user["password"].isString() || user["password"].asString().empty()) {
            
            Json::Value failedUser;
            failedUser["index"] = i;
            failedUser["reason"] = "username or password is empty";
            if (user.isMember("username")) failedUser["username"] = user["username"];
            failedUsers.append(failedUser);
            continue;
        }
        
        std::string name = user["username"].asString();
        std::string password = user["password"].asString();
        std::string uid = user.isMember("uid") && user["uid"].isString() && !user["uid"].asString().empty() 
            ? user["uid"].asString() : Utils::GenerateUid();

        if (Users::FindUserByUid(uid)) {
            Json::Value failedUser;
            failedUser["index"] = i;
            failedUser["username"] = name;
            failedUser["reason"] = "username already existed";
            failedUsers.append(failedUser);
            continue;
        }

        // 添加用户
        bool success = Users::AddUser(name, password, uid);
        
        if (success) {
            Json::Value successUser;
            successUser["username"] = name;
            successUser["uid"] = uid;
            successUsers.append(successUser);
        } else {
            Json::Value failedUser;
            failedUser["index"] = i;
            failedUser["username"] = name;
            failedUser["reason"] = "fail to add user";
            failedUsers.append(failedUser);
        }
    }
    
    response["data"]["success_count"] = static_cast<int>(successUsers.size());
    response["data"]["failed_count"] = static_cast<int>(failedUsers.size());
    response["data"]["success_users"] = successUsers;
    response["data"]["failed_users"] = failedUsers;
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k200OK);
    callback(resp);
}

void DbInfoController::GetOnlineUsers(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    Json::Value response;
    response["code"] = 200;
    response["message"] = "success to get online users";

    try {
        auto user_str = ConnectionManager::GetInstance().GetOnlineUsers();
        response["data"] = user_str;
    }
    catch (const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "fail to get name: " << e.base().what();
        response["code"] = 500;
    }

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    if (response["code"].asUInt() != 200)
    {
        resp->setStatusCode(drogon::k500InternalServerError);
    }
    else
    {
        resp->setStatusCode(drogon::k200OK);
    }
    callback(resp);
}

void DbInfoController::HandleDbInfoOptions(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    LOG_DEBUG << "DbInfoController::HandleDbInfoOptions called for /db_info";
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k204NoContent);
    callback(resp);
}
