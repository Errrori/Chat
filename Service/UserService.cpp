#include "pch.h"
#include "UserService.h"
#include "Common/User.h"
#include "Data/IUserRepository.h"
#include "RedisService.h"
#include "models/Users.h"
#include "auth/TokenFactory.h"
#include "auth/TokenService.h"
#include "auth/TokenConstants.h"

using namespace Utils::Authentication;

namespace
{
    constexpr const char* kRefreshTokenCookieName = "refresh_token";

    drogon::Cookie BuildRefreshTokenCookie(const std::string& token, unsigned ttl_seconds)
    {
        drogon::Cookie cookie(kRefreshTokenCookieName, token);
        cookie.setHttpOnly(true);
        cookie.setPath("/");
        cookie.setMaxAge(static_cast<int>(ttl_seconds));
        return cookie;
    }

    drogon::Cookie BuildClearedRefreshTokenCookie()
    {
        drogon::Cookie cookie(kRefreshTokenCookieName, "");
        cookie.setHttpOnly(true);
        cookie.setPath("/");
        cookie.setMaxAge(0);
        return cookie;
    }
}

drogon::Task<drogon::HttpResponsePtr> UserService::UserRegister(const UserInfo& info) const
{
    const auto& username = info.getUsername();
    const auto& password = info.getHashedPassword();
    const auto& account = info.getAccount();

    // Check essential fields validation
    if (username.empty() || password.empty() || account.empty() || !IsValidAccount(account))
    {
        LOG_ERROR << "UserRegister: lack of essential fields";
        co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");
    }

    try
    {
        auto record = co_await _user_repo->GetUserInfoByAccountCoro(account);
        if (record != Json::nullValue)
            co_return Utils::CreateSuccessResp(400, 400, "user is already existed");

        bool success = co_await _user_repo->AddUserCoro(info);
        if (success)
            co_return Utils::CreateSuccessResp(200, 200, "user register: " + username);
        else
            co_return Utils::CreateErrorResponse(500, 500, "can not create user info");
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "UserRegister exception: " << e.what();
        co_return Utils::CreateErrorResponse(500, 500, "server error");
    }
}

drogon::Task<drogon::HttpResponsePtr> UserService::UserLogin(const UserInfo& info) const
{
    // Validate account and password
    if (info.getAccount().empty() || info.getHashedPassword().empty())
        co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");

    try
    {
        auto record = co_await _user_repo->GetUserInfoByAccountCoro(info.getAccount());
        if (record == Json::nullValue)
            co_return Utils::CreateErrorResponse(400, 400, "user is not existed");

        const auto  user_record = UserInfo::FromJson(record);
        const auto& stored_password = record["password"].asString();

        // Verify password
        if (stored_password != info.getHashedPassword())
            co_return Utils::CreateErrorResponse(400, 400, "password is not correct");

        // Build response data with tokens
        Json::Value data;
        data["uid"] = user_record.getUid();

        auto pair = Auth::TokenFactory::GeneratePair(user_record);
        data["access_token"] = pair.access.value;
        data["access_expires_in"] = pair.access.ttl;
        data["refresh_expires_in"] = pair.refresh.ttl;
        data["token_type"] = "Bearer";

        // Store refresh session and cache user info
        co_await _redis_service->StoreRefreshSession(
            user_record.getUid(), pair.refresh.jti,
            static_cast<int>(Auth::RefreshSessionTTL));

        co_await _redis_service->CacheUserInfo(user_record.getUid(), user_record.ToJson());

        auto resp = Utils::CreateSuccessJsonResp(200, 200, "success login", data);
        resp->addCookie(BuildRefreshTokenCookie(pair.refresh.value, pair.refresh.ttl));
        co_return resp;
    }catch (const std::exception& e)
    {
        LOG_ERROR << "UserLogin exception: " << e.what();
        co_return Utils::CreateErrorResponse(500, 500, "server error");
    }
}

drogon::Task<UserInfo> UserService::GetUserInfo(const std::string& uid)
{
    co_return co_await _user_repo->GetUserInfo(uid);
}

drogon::Task<drogon::HttpResponsePtr> UserService::GetUser(
    std::optional<std::string> uid, std::optional<std::string> account) const
{
    const bool hasUid = uid.has_value() && !uid->empty();
    const bool hasAccount = account.has_value() && !account->empty();

    // Validate query parameters
    if (!hasUid && !hasAccount)
        co_return Utils::CreateErrorResponse(400, 400, "User ID or account can not be empty");

    if (hasUid && hasAccount)
        co_return Utils::CreateErrorResponse(400, 400, "can not query user in two parameters");

    try
    {
        Json::Value record;
        if (hasUid)
            record = co_await _user_repo->GetUserInfoByUidCoro(*uid);
        else
            record = co_await _user_repo->GetUserInfoByAccountCoro(*account);

        if (record == Json::nullValue)
            co_return Utils::CreateErrorResponse(404, 404, "User is not found");

        drogon_model::sqlite3::Users user(record);
        // Field mask for user info response (empty string means hide field)
        static const std::vector<std::string> mask = {
            "",             // id
            "username",
            "account",
            "",             // password
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

        // Build response
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

drogon::Task<drogon::HttpResponsePtr> UserService::ModifyUserInfo(const Json::Value& body) const
{
    // Check required identifier (uid or account)
    if (!body.isMember("account") && !body.isMember("uid"))
        co_return Utils::CreateErrorResponse(400, 400, "Invalid request data");

    // Check modify fields
    const bool hasUsername = body.isMember("username") && !body["username"].isNull();
    const bool hasAvatar = body.isMember("avatar") && !body["avatar"].isNull();
    const bool hasEmail = body.isMember("email") && !body["email"].isNull();
    const bool hasSignature = body.isMember("signature") && !body["signature"].isNull();

    if (!hasUsername && !hasAvatar && !hasEmail && !hasSignature)
        co_return Utils::CreateErrorResponse(400, 400, "fail to modify user's info");

    try
    {
        // SQL statements for update
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

        bool ok = false;

        if (body.isMember("uid"))
        {
            const auto uid = body["uid"].asString();
            auto record = co_await _user_repo->GetUserInfoByUidCoro(uid);
            if (record == Json::nullValue)
                co_return Utils::CreateErrorResponse(404, 404, "user not found");

            // Execute update by uid
            auto r = co_await drogon::app().getDbClient()->execSqlCoro(
                kSqlUpdateByUid,
                hasUsername ? body["username"].asString() : nullptr,
                hasAvatar ? body["avatar"].asString() : nullptr,
                hasEmail ? body["email"].asString() : nullptr,
                hasSignature ? body["signature"].asString() : nullptr,
                uid
            );
            ok = (r.affectedRows() > 0);

            // Invalidate cached user info if update success
            if (ok)
                co_await _redis_service->InvalidateUserInfo(uid);
        }
        else
        {
            const auto account = body["account"].asString();
            // Execute update by account
            auto r = co_await drogon::app().getDbClient()->execSqlCoro(
                kSqlUpdateByAccount,
                hasUsername ? body["username"].asString() : nullptr,
                hasAvatar ? body["avatar"].asString() : nullptr,
                hasEmail ? body["email"].asString() : nullptr,
                hasSignature ? body["signature"].asString() : nullptr,
                account
            );
            ok = (r.affectedRows() > 0);
        }

        if (!ok)
            co_return Utils::CreateErrorResponse(400, 400, "fail to modify user's info");

        // Build success response
        Json::Value response;
        response["code"] = 200;
        response["message"] = "success to modify user's info";
        response["data"] = body;
        co_return drogon::HttpResponse::newHttpJsonResponse(response);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "ModifyUserInfo exception: " << e.what();
        co_return Utils::CreateErrorResponse(400, 400, "fail to modify user's info");
    }
}

drogon::Task<drogon::HttpResponsePtr> UserService::RefreshToken(
    const std::string& refresh_token) const
{
    UserInfo info;
    std::string old_jti;
    // Verify refresh token
    if (!Auth::TokenService::GetInstance().Verify(
        refresh_token, Auth::TokenType::Refresh, info, old_jti))
    {
        co_return Utils::CreateErrorResponse(401, 401, "invalid or expired refresh token");
    }

    const auto& uid = info.getUid();
    if (uid.empty() || old_jti.empty())
        co_return Utils::CreateErrorResponse(401, 401, "invalid refresh token claims");

    // Get user info from cache or DB
    auto cached = co_await _redis_service->GetCachedUserInfo(uid);
    UserInfo full_info;
    if (cached != Json::nullValue)
    {
        full_info = UserInfo::FromJson(cached);
    }
    else
    {
        auto record = co_await _user_repo->GetUserInfo(uid);
        if (record.getUid().empty())
            co_return Utils::CreateErrorResponse(401, 401, "user not found");
        full_info = record;
    }

    // Generate new tokens
    auto new_access = Auth::TokenFactory::GenerateAccess(full_info);
    auto new_refresh = Auth::TokenFactory::GenerateRefresh(uid);

    // Rotate refresh session in Redis
    bool rotated = co_await _redis_service->RotateRefreshSession(
        uid, old_jti, new_refresh.jti,
        static_cast<int>(Auth::RefreshSessionTTL));

    if (!rotated)
    {
        LOG_WARN << "[UserService] RefreshToken: stale jti for uid=" << uid;
        co_return Utils::CreateErrorResponse(401, 401, "refresh token has been invalidated");
    }

    // Build response with new tokens
    Json::Value data;
    data["access_token"] = new_access.value;
    data["access_expires_in"] = new_access.ttl;
    data["refresh_expires_in"] = new_refresh.ttl;
    data["token_type"] = "Bearer";
    auto resp = Utils::CreateSuccessJsonResp(200, 200, "token refreshed", data);
    resp->addCookie(BuildRefreshTokenCookie(new_refresh.value, new_refresh.ttl));
    co_return resp;
}

drogon::Task<drogon::HttpResponsePtr> UserService::Logout(
    const std::string& refresh_token) const
{
    UserInfo info;
    std::string jti;
    // Verify refresh token (ignore failure for logout)
    if (!Auth::TokenService::GetInstance().Verify(
        refresh_token, Auth::TokenType::Refresh, info, jti))
    {
        auto resp = Utils::CreateSuccessResp(200, 200, "logged out");
        resp->addCookie(BuildClearedRefreshTokenCookie());
        co_return resp;
    }

    // Revoke refresh session if user exists
    if (!info.getUid().empty())
        co_await _redis_service->RevokeRefreshSession(info.getUid());

    auto resp = Utils::CreateSuccessResp(200, 200, "logged out");
    resp->addCookie(BuildClearedRefreshTokenCookie());
    co_return resp;
}