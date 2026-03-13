#include "pch.h"
#include "UserService.h"
#include "Common/User.h"
#include "Data/IUserRepository.h"
#include "RedisService.h"
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
        auto record = co_await _user_repo->GetUserByAccount(account);
        if (!record.getUid().empty())
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
    if (info.getAccount().empty() || info.getHashedPassword().empty())
        co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");

    try
    {
        // Verify password against stored hash
        auto stored_password = co_await _user_repo->GetHashedPassword(info.getAccount());
        if (stored_password.empty())
            co_return Utils::CreateErrorResponse(400, 400, "user is not existed");

        if (stored_password != info.getHashedPassword())
            co_return Utils::CreateErrorResponse(400, 400, "password is not correct");

        // Fetch public profile (uid/username/account/avatar)
        auto user_record = co_await _user_repo->GetUserByAccount(info.getAccount());
        if (user_record.getUid().empty())
            co_return Utils::CreateErrorResponse(500, 500, "server error");

        auto pair = Auth::TokenFactory::GeneratePair(user_record);

        Json::Value data;
        data["uid"]               = user_record.getUid();
        data["access_token"]      = pair.access.value;
        data["access_expires_in"] = pair.access.ttl;
        data["refresh_expires_in"]= pair.refresh.ttl;

        co_await _redis_service->StoreRefreshSession(
            user_record.getUid(), pair.refresh.jti,
            static_cast<int>(Auth::RefreshSessionTTL));

        co_await _redis_service->CacheUserInfo(user_record.getUid(), user_record);

        auto resp = Utils::CreateSuccessJsonResp(200, 200, "success login", data);
        resp->addCookie(BuildRefreshTokenCookie(pair.refresh.value, pair.refresh.ttl));
        co_return resp;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "UserLogin exception: " << e.what();
        co_return Utils::CreateErrorResponse(500, 500, "server error");
    }
}

drogon::Task<UserInfo> UserService::GetUserInfo(const std::string& uid)
{
    auto cached = co_await _redis_service->GetCachedUserInfo(uid);
    if (!cached.getUid().empty())
        co_return cached;

    auto record = co_await _user_repo->GetUserByUid(uid);
    if (!record.getUid().empty())
        co_await _redis_service->CacheUserInfo(uid, record);
    co_return record;
}

drogon::Task<UserService::GetUserResult> UserService::GetUser(
    std::optional<std::string> uid, std::optional<std::string> account) const
{
    GetUserResult result;

    const bool hasUid = uid.has_value() && !uid->empty();
    const bool hasAccount = account.has_value() && !account->empty();

    // Validate query parameters
    if (!hasUid && !hasAccount)
    {
        result.http_status = 400;
        result.message = "User ID or account can not be empty";
        co_return result;
    }

    if (hasUid && hasAccount)
    {
        result.http_status = 400;
        result.message = "can not query user in two parameters";
        co_return result;
    }

    try
    {
        UserInfo record;
        if (hasUid)
            record = co_await _user_repo->GetUserByUid(*uid);
        else
            record = co_await _user_repo->GetUserByAccount(*account);

        if (record.getUid().empty())
        {
            result.http_status = 404;
            result.message = "User is not found";
            co_return result;
        }

        result.ok = true;
        result.http_status = 200;
        result.message = "success to get user info";
        result.user = std::move(record);
        co_return result;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "exception in GetUser: " << e.what();
        result.http_status = 500;
        result.message = "server error";
        co_return result;
    }
}

drogon::Task<UserService::ModifyUserResult> UserService::ModifyUserInfo(const Json::Value& body) const
{
    ModifyUserResult result;

    if (!body.isMember("account") && !body.isMember("uid"))
    {
        result.http_status = 400;
        result.message = "Invalid request data";
        co_return result;
    }

    const bool hasUsername = body.isMember("username") && !body["username"].isNull();
    const bool hasAvatar   = body.isMember("avatar")   && !body["avatar"].isNull();

    if (!hasUsername && !hasAvatar)
    {
        result.http_status = 400;
        result.message = "no updatable fields provided";
        co_return result;
    }

    try
    {
        const char* kSqlUpdateByUid =
            "UPDATE users SET "
            "username = COALESCE(?, username), "
            "avatar   = COALESCE(?, avatar) "
            "WHERE uid = ?";

        const char* kSqlUpdateByAccount =
            "UPDATE users SET "
            "username = COALESCE(?, username), "
            "avatar   = COALESCE(?, avatar) "
            "WHERE account = ?";

        bool ok = false;

        if (body.isMember("uid"))
        {
            const auto uid = body["uid"].asString();
            auto record = co_await _user_repo->GetUserByUid(uid);
            if (record.getUid().empty())
            {
                result.http_status = 404;
                result.message = "user not found";
                co_return result;
            }

            auto r = co_await drogon::app().getDbClient()->execSqlCoro(
                kSqlUpdateByUid,
                hasUsername ? body["username"].asString() : nullptr,
                hasAvatar   ? body["avatar"].asString()   : nullptr,
                uid
            );
            ok = (r.affectedRows() > 0);
            if (ok)
                co_await _redis_service->InvalidateUserInfo(uid);

            result.data["uid"] = uid;
        }
        else
        {
            const auto account = body["account"].asString();
            auto record = co_await _user_repo->GetUserByAccount(account);
            if (record.getUid().empty())
            {
                result.http_status = 404;
                result.message = "user not found";
                co_return result;
            }

            auto r = co_await drogon::app().getDbClient()->execSqlCoro(
                kSqlUpdateByAccount,
                hasUsername ? body["username"].asString() : nullptr,
                hasAvatar   ? body["avatar"].asString()   : nullptr,
                account
            );
            ok = (r.affectedRows() > 0);
            if (ok)
                co_await _redis_service->InvalidateUserInfo(record.getUid());

            result.data["uid"] = record.getUid();
        }

        if (!ok)
        {
            result.http_status = 400;
            result.message = "fail to modify user's info";
            co_return result;
        }

        if (hasUsername)
            result.data["username"] = body["username"];
        if (hasAvatar)
            result.data["avatar"] = body["avatar"];

        result.ok = true;
        result.http_status = 200;
        result.message = "success to modify user's info";
        co_return result;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "ModifyUserInfo exception: " << e.what();
        result.http_status = 500;
        result.message = "fail to modify user's info";
        co_return result;
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

    // Verify user still exists
    auto check = co_await _user_repo->GetUserByUid(uid);
    if (check.getUid().empty())
        co_return Utils::CreateErrorResponse(401, 401, "user not found");

    // Generate new tokens
    auto new_access = Auth::TokenFactory::GenerateAccess(uid);
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