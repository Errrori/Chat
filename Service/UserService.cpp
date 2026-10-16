#include "pch.h"
#include <cstdlib>
#include "Common/ResponseHelper.h"
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

    // 跨平台读取环境变量（避免 MSVC 对 std::getenv 的安全告警）
    std::string GetEnvVar(const char* name)
    {
#ifdef _WIN32
        char* value = nullptr;
        size_t len = 0;
        if (_dupenv_s(&value, &len, name) == 0 && value != nullptr)
        {
            std::string result(value);
            free(value);
            return result;
        }
        return {};
#else
        const char* value = std::getenv(name);
        return value ? std::string(value) : std::string{};
#endif
    }

    // /user/profile 读缓存开关：默认开启；设置 PROFILE_CACHE=0/false/off/no 可关闭，
    // 用于压测时做「缓存开 vs 关」的对照实验。
    bool ProfileCacheEnabled()
    {
        static const bool enabled = []() {
            const std::string v = GetEnvVar("PROFILE_CACHE");
            if (v.empty())
                return true;
            return !(v == "0" || v == "false" || v == "off" || v == "no");
        }();
        return enabled;
    }

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
    const auto& username = info.GetUsername();
    const auto& password = info.GetHashedPassword();
    const auto& account = info.GetAccount();

    // Check essential fields validation
    if (!username || !password || !account)
    {
        LOG_ERROR << "UserRegister: lack of essential fields";
        co_return ResponseHelper::MakeResponse(400, 400, "lack of essential fields");
    }

    if (!IsValidAccount(*account))
	{
        LOG_ERROR << " account is not valid";
        co_return ResponseHelper::MakeResponse(400, 400, "account is not valid");
	}

    try
    {
        auto status = co_await _user_repo->AddUserCoro(info);

        switch (status)
        {
        case AddUserStatus::Inserted:
            co_return ResponseHelper::MakeResponse(200, 200, "user register: " + *username);
        case AddUserStatus::AlreadyExists:
            co_return ResponseHelper::MakeResponse(409, 409, "account already exists");
        default:
            co_return ResponseHelper::MakeResponse(500, 500, "can not create user info");
        }
    }
    catch (const std::exception& e)
    {
        co_return ResponseHelper::MakeResponse(500, 500, "server error");
    }
}

drogon::Task<drogon::HttpResponsePtr> UserService::UserLogin(const UserInfo& info) const
{
    if (!info.GetAccount() || !info.GetHashedPassword())
        co_return ResponseHelper::MakeResponse(400, 400, "lack of essential fields");

    const auto& account = info.GetAccount();

    try
    {
        // Verify password against stored hash
        auto stored_password = co_await _user_repo->GetHashedPassword(*account);
        if (stored_password.empty())
            co_return ResponseHelper::MakeResponse(400, 400, "user is not existed");

        if (stored_password != info.GetHashedPassword())
            co_return ResponseHelper::MakeResponse(400, 400, "password is not correct");

        // Fetch public profile (uid/username/account/avatar)
        auto user_record = co_await _user_repo->GetAuthUserByAccount(*account);
        if (!user_record.IsValid())
            co_return ResponseHelper::MakeResponse(500, 500, "server error");
        auto uid = *user_record.GetUid();
        auto at = Auth::TokenFactory::GenerateToken(uid,Auth::TokenType::Access);
		auto rt = Auth::TokenFactory::GenerateToken(uid,Auth::TokenType::Refresh);

        Json::Value data;
        data["uid"]               = uid;
        data["token"]             = at.value;
        data["token_type"]        = "Bearer";

        co_await _redis_service->StoreRefreshSession(
            uid, rt.jti,
            static_cast<int>(Auth::RefreshSessionTTL));

        auto display_profile = UserInfoBuilder::BuildCached(
            *user_record.GetUsername(), *user_record.GetAvatar());

        co_await _redis_service->CacheDisplayProfile(uid, display_profile);

        auto resp = ResponseHelper::MakeResponse(200, 200, "success login", data);
        resp->addCookie(BuildRefreshTokenCookie(rt.value, Auth::RefreshTokenTTL));

        co_return resp;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "UserLogin exception: " << e.what();
        co_return ResponseHelper::MakeResponse(500, 500, "server error");
    }
}

drogon::Task<UserInfo> UserService::GetDisplayProfileByUid(const std::string& uid) const
{
    auto cached = co_await _redis_service->GetCachedDisplayProfile(uid);
    if (cached.IsValid())
        co_return cached;

    auto record = co_await _user_repo->GetDisplayProfileByUid(uid);
    if (record.IsValid())
        co_await _redis_service->CacheDisplayProfile(uid, record);
    co_return record;
}

drogon::Task<UserInfo> UserService::GetUserProfileByUid(const std::string& uid) const
{
    if (uid.empty())
        co_return UserInfo{};

    try
    {
        const bool cache_on = ProfileCacheEnabled();

        // 先查缓存，命中直接返回，避免每次请求都打库
        if (cache_on)
        {
            auto cached = co_await _redis_service->GetCachedUserProfile(uid);
            if (cached.IsValid())
                co_return cached;
        }

        auto record = co_await _user_repo->GetUserProfileByUid(uid);

        // 回填缓存（cache-aside），TTL = UserInfoTTL
        if (cache_on && record.IsValid())
            co_await _redis_service->CacheUserProfile(uid, record);

        co_return record;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "exception in GetUserProfileByUid: " << e.what();
        co_return UserInfo{};
    }
}

drogon::Task<UserInfo> UserService::SearchUserByAccount(const std::string& account) const
{
    if (account.empty())
        co_return UserInfo{};

    try
    {
        auto record = co_await _user_repo->FindUserByAccount(account);
        co_return record;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "SearchUserByAccount exception: " << e.what();
        co_return UserInfo{};
    }
}

drogon::Task<UserInfo> UserService::UpdateUserProfile(
    const std::string& uid, const UserInfo& update_info) const
{
    if (uid.empty())
        co_return UserInfo{};

    if (!update_info.HasUpdates())
        co_return UserInfo{};

    try
    {
        auto existing = co_await _user_repo->GetUserProfileByUid(uid);
        if (!existing.IsValid())
            co_return UserInfo{};

        auto ok = co_await _user_repo->UpdateUserProfile(uid, update_info);
        if (!ok)
            co_return UserInfo{};

        // username/avatar/signature 均在完整资料缓存中，任一更新都要失效
        co_await _redis_service->InvalidateUserProfile(uid);

        if (update_info.AffectsDisplayProfile())
            co_await _redis_service->InvalidateDisplayProfile(uid);

        auto updated = co_await _user_repo->GetUserProfileByUid(uid);
        co_return updated;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "UpdateUserProfile exception: " << e.what();
        co_return UserInfo{};
    }
}

drogon::Task<drogon::HttpResponsePtr> UserService::RefreshToken(
    const std::string& refresh_token) const
{
    // Verify refresh token
    auto token = Auth::TokenService::GetInstance().Verify(
        refresh_token, Auth::TokenType::Refresh);

    if (!token)
    {
        co_return ResponseHelper::MakeResponse(401, 401, "invalid or expired refresh token");
    }

    // Verify user still exists
    auto check = co_await _user_repo->GetUserProfileByUid(token->uid);
    if (!check.IsValid())
        co_return ResponseHelper::MakeResponse(401, 401, "user not found");

    // Generate new tokens
    auto new_access = Auth::TokenFactory::GenerateToken(token->uid, Auth::TokenType::Access);
    auto new_refresh = Auth::TokenFactory::GenerateToken(token->uid,Auth::TokenType::Refresh);

    // Rotate refresh session in Redis
    bool rotated = co_await _redis_service->RotateRefreshSession(
        token->uid, token->jti, new_refresh.jti,
        static_cast<int>(Auth::RefreshSessionTTL));

    if (!rotated)
    {
        LOG_WARN << "[UserService] RefreshToken: stale jti for uid=" << token->uid;
        co_return ResponseHelper::MakeResponse(401, 401, "refresh token has been invalidated");
    }

    // Build response with new tokens
    Json::Value data;
    data["access_token"] = new_access.value;
    data["token_type"] = "Bearer";
    auto resp = ResponseHelper::MakeResponse(200, 200, "token refreshed", data);
    resp->addCookie(BuildRefreshTokenCookie(new_refresh.value, Auth::RefreshTokenTTL));
    co_return resp;
}

drogon::Task<drogon::HttpResponsePtr> UserService::Logout(
    const std::string& refresh_token) const
{
    // Verify refresh token (ignore failure for logout)
    auto token = Auth::TokenService::GetInstance().Verify(
        refresh_token, Auth::TokenType::Refresh);
    if (!token)
    {
        auto resp = ResponseHelper::MakeResponse(200, 200, "logged out");
        resp->addCookie(BuildClearedRefreshTokenCookie());
        co_return resp;
    }

    // Revoke refresh session if user exists
    co_await _redis_service->RevokeRefreshSession(token->uid);

    // 用户级吊销：作废该用户在登出前签发的所有 access token
    co_await _redis_service->RevokeAccessTokens(token->uid);

    auto resp = ResponseHelper::MakeResponse(200, 200, "logged out");
    resp->addCookie(BuildClearedRefreshTokenCookie());
    co_return resp;
}