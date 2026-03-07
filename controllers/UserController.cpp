#include "pch.h"
#include "UserController.h"

#include "Data/DbAccessor.h"
#include "models/Users.h"
#include "Common/User.h"
#include "models/PrivateChats.h"
#include "Container.h"
#include "Service/RedisService.h"

using namespace drogon;

drogon::Task<HttpResponsePtr> UserController::GetUser(HttpRequestPtr req)
{
    LOG_TRACE << "Accessing get user by id route";

    try
    {
        auto uidOpt = req->getOptionalParameter<std::string>("uid");
        auto accountOpt = req->getOptionalParameter<std::string>("account");

        const bool hasUid = uidOpt.has_value() && !uidOpt->empty();
        const bool hasAccount = accountOpt.has_value() && !accountOpt->empty();

        if (!hasUid && !hasAccount)
            co_return Utils::CreateErrorResponse(400, 400, "User ID or account can not be empty");

        if (hasUid && hasAccount)
            co_return Utils::CreateErrorResponse(400, 400, "can not query user in two parameters");

        auto client = DbAccessor::GetDbClient();

        const char* sql = nullptr;
        std::string key;
        if (hasUid) {
            sql = "SELECT * FROM users WHERE uid = ? LIMIT 1";
            key = *uidOpt;
        } else {
            sql = "SELECT * FROM users WHERE account = ? LIMIT 1";
            key = *accountOpt;
        }

        auto result = co_await client->execSqlCoro(sql, key);

        if (result.size() == 0)
            co_return Utils::CreateErrorResponse(404, 404, "User is not found");

        drogon_model::sqlite3::Users user(result[0], -1);
        // �������ݲ����������ֶΣ������� password��
        static const std::vector<std::string> mask = {
            "",             // id�������أ�
            "username",
            "account",
            "",             // password�������أ�
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

        Json::Value response;
        response["code"] = 200;
        response["message"] = "success to get user info";
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

    // ������ԭʵ��һ�µ�У���뷵��
    auto json_body = req->getJsonObject();
    if (!json_body) {
        response["code"] = 400;
        response["message"] = "request format error";
        co_return drogon::HttpResponse::newHttpJsonResponse(response);
    }

    // �����ṩ uid �� account����ԭ�߼�һ�£�
    if (!json_body->isMember("account") && !json_body->isMember("uid")) {
        response["code"] = 400;
        response["message"] = "Invalid request data";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k400BadRequest);
        co_return resp;
    }

    // ��־����һ��
    auto json_data = UserInfo::FromJson(*json_body);
    LOG_INFO << json_data.ToString();




    // Э�̸��£������������г��ֵ��ֶΣ�username/avatar/email/signature��
    // δ���ֵ��ֶ�ʹ�� COALESCE(?, col) + �� nullptr �ﵽ�������¡���Ч��
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

    // ��û���κοɸ����ֶ�ʱ��������ԭʵ��һ�£��ж�Ϊ�޸�ʧ��
    if (!hasUsername && !hasAvatar && !hasEmail && !hasSignature) {
        response["code"] = 400;
        response["message"] = "fail to modify user's info ";
        co_return drogon::HttpResponse::newHttpJsonResponse(response);
    }

    try {
        bool ok = false;

        if (json_body->isMember("uid")) {
            const auto uid = (*json_body)["uid"].asString();
            auto r = co_await client->execSqlCoro(
                kSqlUpdateByUid,
                hasUsername ? (*json_body)["username"].asString() : nullptr,
                hasAvatar ? (*json_body)["avatar"].asString() : nullptr,
                hasEmail ? (*json_body)["email"].asString() : nullptr,
                hasSignature ? (*json_body)["signature"].asString() : nullptr,
                uid
            );
            ok = (r.affectedRows() > 0);
        }
        else {
            const auto account = (*json_body)["account"].asString();
            auto r = co_await client->execSqlCoro(
                kSqlUpdateByAccount,
                hasUsername ? (*json_body)["username"].asString() : nullptr,
                hasAvatar ? (*json_body)["avatar"].asString() : nullptr,
                hasEmail ? (*json_body)["email"].asString() : nullptr,
                hasSignature ? (*json_body)["signature"].asString() : nullptr,
                account
            );
            ok = (r.affectedRows() > 0);
        }

        if (!ok) {
            response["code"] = 400;
            response["message"] = "fail to modify user's info ";
            co_return drogon::HttpResponse::newHttpJsonResponse(response);
        }

        response["code"] = 200;
        response["message"] = "success to modify user's info ";
        response["data"] = *json_body;

        // 修改资料后使 Redis 缓存失效
        const auto uid_to_invalidate = json_body->isMember("uid")
            ? (*json_body)["uid"].asString()
            : std::string{};
        if (!uid_to_invalidate.empty())
        {
            drogon::async_run([uid_to_invalidate]() -> drogon::Task<>
            {
                co_await GET_REDIS_SERVICE->InvalidateUserInfo(uid_to_invalidate);
            }());
        }

        co_return drogon::HttpResponse::newHttpJsonResponse(response);
    }
    catch (const std::exception& e) {
    	LOG_ERROR << "ModifyUserInfo exception: " << e.what();
        response["code"] = 400;
        response["message"] = "fail to modify user's info ";
        co_return drogon::HttpResponse::newHttpJsonResponse(response);
    }
}


// ... existing code ...
