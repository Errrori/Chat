#include "pch.h"
#include "UserController.h"

#include "Data/DbAccessor.h"
#include "models/Users.h"
#include "Common/User.h"

using namespace drogon;

drogon::Task<HttpResponsePtr> UserController::GetUser(HttpRequestPtr req)
{
    LOG_TRACE << "Accessing get user by id route";

    try
    {
        // 参数获取与校验（与原函数一致）
        auto uidOpt = req->getOptionalParameter<std::string>("uid");
        auto accountOpt = req->getOptionalParameter<std::string>("account");

        if (!uidOpt.has_value() && !accountOpt.has_value())
            co_return Utils::CreateErrorResponse(400, 400, "User ID or account can not be empty");

        if (uidOpt.has_value() && accountOpt.has_value())
            co_return Utils::CreateErrorResponse(400, 400, "can not query user in two parameters");

        // 查询
        auto client = DbAccessor::GetDbClient();
        drogon::orm::Result result;

        if (uidOpt.has_value())
        {
            result = co_await client->execSqlCoro(
                "SELECT * FROM users WHERE uid = ? LIMIT 1", uidOpt.value());
        }
        else
        {
            result = co_await client->execSqlCoro(
                "SELECT * FROM users WHERE account = ? LIMIT 1", accountOpt.value());
        }

        if (result.size() == 0)
            co_return Utils::CreateErrorResponse(404, 404, "User is not found");

        drogon_model::sqlite3::Users user(result[0], -1);
        // 构造数据并过滤敏感字段（不返回 password）
        static const std::vector<std::string> mask = {
            "",             // id（不返回）
            "username",
            "account",
            "",             // password（不返回）
            "uid",
            "avatar",
            "create_time",
            "signature",
            "last_login_time",
            "posts",
            "level",
            "status",
            "email",
            "followers",
            "following"
        };
        Json::Value userInfo = user.toMasqueradedJson(mask);

        // 响应（与原函数一致）
        Json::Value response;
        response["code"] = 200;
        response["message"] = "Success";
        response["data"] = userInfo;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k200OK);
        co_return resp;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "exception in GetUser: " << e.what();
        co_return Utils::CreateErrorResponse(500, 500, "server error");
    }
}

Task<HttpResponsePtr> UserController::ModifyUserInfo(drogon::HttpRequestPtr req)
{
    Json::Value response;

    // 保持与原实现一致的校验与返回
    auto json_body = req->getJsonObject();
    if (!json_body) {
        response["code"] = 400;
        response["message"] = "request format error";
        co_return drogon::HttpResponse::newHttpJsonResponse(response);
    }

    // 至少提供 uid 或 account（与原逻辑一致）
    if (!json_body->isMember("account") && !json_body->isMember("uid")) {
        response["code"] = 400;
        response["message"] = "Invalid request data";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        co_return resp;
    }

    // 日志保持一致
    auto json_data = UserInfo::FromJson(*json_body);
    LOG_INFO << json_data.ToString();




    // 协程更新：仅更新请求中出现的字段（username/avatar/email/signature）
    // 未出现的字段使用 COALESCE(?, col) + 传 nullptr 达到“不更新”的效果
    const char* kSqlUpdateByUid =
        "UPDATE users SET "
        "username = COALESCE(?, username), "
        "avatar = COALESCE(?, avatar), "
        "email = COALESCE(?, email), "
        "signature = COALESCE(?, signature) "
        "WHERE uid = ?";

    const char* kSqlUpdateByAccount =
        "UPDATE users SET "
        "username = COALESCE(?, username), "
        "avatar = COALESCE(?, avatar), "
        "email = COALESCE(?, email), "
        "signature = COALESCE(?, signature) "
        "WHERE account = ?";

    auto client = DbAccessor::GetDbClient();

    const bool hasUsername = json_body->isMember("username") && !(*json_body)["username"].isNull();
    const bool hasAvatar = json_body->isMember("avatar") && !(*json_body)["avatar"].isNull();
    const bool hasEmail = json_body->isMember("email") && !(*json_body)["email"].isNull();
    const bool hasSignature = json_body->isMember("signature") && !(*json_body)["signature"].isNull();

    // 当没有任何可更新字段时，保持与原实现一致：判定为修改失败
    if (!hasUsername && !hasAvatar && !hasEmail && !hasSignature) {
        response["code"] = 400;
        response["message"] = "fail to modify user's info ";
        co_return drogon::HttpResponse::newHttpJsonResponse(response);
    }

    try {
        drogon::orm::Result r;

        if (json_body->isMember("uid")) {
            const auto uid = (*json_body)["uid"].asString();
            r = co_await client->execSqlCoro(
                kSqlUpdateByUid,
                hasUsername ? (*json_body)["username"].asString() : nullptr,
                hasAvatar ? (*json_body)["avatar"].asString() : nullptr,
                hasEmail ? (*json_body)["email"].asString() : nullptr,
                hasSignature ? (*json_body)["signature"].asString() : nullptr,
                uid
            );
        }
        else {
            const auto account = (*json_body)["account"].asString();
            r = co_await client->execSqlCoro(
                kSqlUpdateByAccount,
                hasUsername ? (*json_body)["username"].asString() : nullptr,
                hasAvatar ? (*json_body)["avatar"].asString() : nullptr,
                hasEmail ? (*json_body)["email"].asString() : nullptr,
                hasSignature ? (*json_body)["signature"].asString() : nullptr,
                account
            );
        }

        const bool result = (r.affectedRows() > 0);

        if (!result) {
            response["code"] = 400;
            response["message"] = "fail to modify user's info ";
            co_return drogon::HttpResponse::newHttpJsonResponse(response);
        }

        response["code"] = 200;
        response["message"] = "success to modify user's info ";
        response["data"] = *json_body;
        co_return drogon::HttpResponse::newHttpJsonResponse(response);
    }
    catch (const std::exception& e) {
        // 原实现未专门分支处理系统错误，此处与失败分支保持一致返回 400
        LOG_ERROR << "ModifyUserInfo exception: " << e.what();
        response["code"] = 400;
        response["message"] = "fail to modify user's info ";
        co_return drogon::HttpResponse::newHttpJsonResponse(response);
    }
}
// ... existing code ...