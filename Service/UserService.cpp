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
    int64_t NowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

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
    const auto t0 = NowMs();
    const auto& username = info.getUsername();
    const auto& password = info.getHashedPassword();
    const auto& account = info.getAccount();

    // Check essential fields validation
    if (username.empty() || password.empty() || account.empty())
    {
        LOG_ERROR << "UserRegister: lack of essential fields";
        co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");
    }

    if (!IsValidAccount(account))
	{
        LOG_ERROR << " account is not valid";
        co_return Utils::CreateErrorResponse(400, 400, "account is not valid");
	}

    try
    {
        const auto t_check_start = NowMs();
        auto record = co_await _user_repo->GetAuthUserByAccount(account);
        LOG_INFO << "[UserService] UserRegister check-account cost_ms=" << (NowMs() - t_check_start)
                 << " account=" << account;
        if (!record.getUid().empty())
        {
            LOG_INFO << "[UserService] UserRegister duplicate-account total_cost_ms=" << (NowMs() - t0)
                     << " account=" << account;
            co_return Utils::CreateSuccessResp(400, 400, "user is already existed");
        }

        const auto t_add_start = NowMs();
        bool success = co_await _user_repo->AddUserCoro(info);
        LOG_INFO << "[UserService] UserRegister add-user cost_ms=" << (NowMs() - t_add_start)
                 << " account=" << account;

        if (success)
        {
            LOG_INFO << "[UserService] UserRegister success total_cost_ms=" << (NowMs() - t0)
                     << " account=" << account;
            co_return Utils::CreateSuccessResp(200, 200, "user register: " + username);
        }
        else
        {
            LOG_WARN << "[UserService] UserRegister failed total_cost_ms=" << (NowMs() - t0)
                     << " account=" << account;
            co_return Utils::CreateErrorResponse(500, 500, "can not create user info");
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "UserRegister exception: " << e.what() << " total_cost_ms=" << (NowMs() - t0)
                  << " account=" << account;
        co_return Utils::CreateErrorResponse(500, 500, "server error");
    }
}

drogon::Task<drogon::HttpResponsePtr> UserService::UserLogin(const UserInfo& info) const
{
    const auto t0 = NowMs();
    if (info.getAccount().empty() || info.getHashedPassword().empty())
        co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");

    const auto& account = info.getAccount();

    try
    {
        // Verify password against stored hash
        const auto t_pwd_start = NowMs();
        auto stored_password = co_await _user_repo->GetHashedPassword(account);
        LOG_INFO << "[UserService] UserLogin get-hash cost_ms=" << (NowMs() - t_pwd_start)
                 << " account=" << account;
        if (stored_password.empty())
        {
            LOG_INFO << "[UserService] UserLogin account-not-found total_cost_ms=" << (NowMs() - t0)
                     << " account=" << account;
            co_return Utils::CreateErrorResponse(400, 400, "user is not existed");
        }

        if (stored_password != info.getHashedPassword())
        {
            LOG_INFO << "[UserService] UserLogin password-mismatch total_cost_ms=" << (NowMs() - t0)
                     << " account=" << account;
            co_return Utils::CreateErrorResponse(400, 400, "password is not correct");
        }

        // Fetch public profile (uid/username/account/avatar)
        const auto t_profile_start = NowMs();
        auto user_record = co_await _user_repo->GetAuthUserByAccount(account);
        LOG_INFO << "[UserService] UserLogin get-user-profile cost_ms=" << (NowMs() - t_profile_start)
                 << " account=" << account;
        if (user_record.getUid().empty())
            co_return Utils::CreateErrorResponse(500, 500, "server error");

        auto pair = Auth::TokenFactory::GeneratePair(user_record.getUid());

        Json::Value data;
        data["uid"]               = user_record.getUid();
        data["access_token"]      = pair.access.value;
        data["access_expires_in"] = pair.access.ttl;
        data["refresh_expires_in"]= pair.refresh.ttl;
        data["token_type"]        = "Bearer";

        const auto t_redis_session_start = NowMs();
        co_await _redis_service->StoreRefreshSession(
            user_record.getUid(), pair.refresh.jti,
            static_cast<int>(Auth::RefreshSessionTTL));
        LOG_INFO << "[UserService] UserLogin redis-store-refresh cost_ms=" << (NowMs() - t_redis_session_start)
                 << " uid=" << user_record.getUid();

        auto display_profile = UserInfoBuilder::BuildCached(
            user_record.getUsername(), user_record.getAvatar());

        const auto t_redis_profile_start = NowMs();
        co_await _redis_service->CacheDisplayProfile(user_record.getUid(), display_profile);
        LOG_INFO << "[UserService] UserLogin redis-cache-display cost_ms=" << (NowMs() - t_redis_profile_start)
                 << " uid=" << user_record.getUid();

        auto resp = Utils::CreateSuccessJsonResp(200, 200, "success login", data);
        resp->addCookie(BuildRefreshTokenCookie(pair.refresh.value, pair.refresh.ttl));
        LOG_INFO << "[UserService] UserLogin success total_cost_ms=" << (NowMs() - t0)
                 << " uid=" << user_record.getUid() << " account=" << account;
        co_return resp;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "UserLogin exception: " << e.what() << " total_cost_ms=" << (NowMs() - t0)
                  << " account=" << account;
        co_return Utils::CreateErrorResponse(500, 500, "server error");
    }
}

drogon::Task<UsersInfo> UserService::GetDisplayProfileByUid(const std::string& uid)
{
    auto cached = co_await _redis_service->GetCachedDisplayProfile(uid);
    if (cached.IsValid())
        co_return cached;

    auto record = co_await _user_repo->GetDisplayProfileByUid(uid);
    if (record.IsValid())
        co_await _redis_service->CacheDisplayProfile(uid, record);
    co_return record;
}

drogon::Task<UsersInfo> UserService::GetUserProfileByUid(const std::string& uid) const
{
    if (uid.empty())
        co_return UsersInfo{};

    try
    {
        auto record = co_await _user_repo->GetUserProfileByUid(uid);
        co_return record;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "exception in GetUserProfileByUid: " << e.what();
        co_return UsersInfo{};
    }
}

drogon::Task<UsersInfo> UserService::SearchUserByAccount(const std::string& account) const
{
    if (account.empty())
        co_return UsersInfo{};

    try
    {
        auto record = co_await _user_repo->FindUserByAccount(account);
        co_return record;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "SearchUserByAccount exception: " << e.what();
        co_return UsersInfo{};
    }
}

drogon::Task<UsersInfo> UserService::UpdateUserProfile(
    const std::string& uid, const UsersInfo& update_info) const
{
    if (uid.empty())
        co_return UsersInfo{};

    if (!update_info.HasUpdates())
        co_return UsersInfo{};

    try
    {
        auto existing = co_await _user_repo->GetUserProfileByUid(uid);
        if (!existing.IsValid())
            co_return UsersInfo{};

        auto ok = co_await _user_repo->UpdateUserProfile(uid, update_info);
        if (!ok)
            co_return UsersInfo{};

        if (update_info.AffectsDisplayProfile())
            co_await _redis_service->InvalidateDisplayProfile(uid);

        auto updated = co_await _user_repo->GetUserProfileByUid(uid);
        co_return updated;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "UpdateUserProfile exception: " << e.what();
        co_return UsersInfo{};
    }
}

drogon::Task<drogon::HttpResponsePtr> UserService::RefreshToken(
    const std::string& refresh_token) const
{
    std::string uid;
    std::string old_jti;
    // Verify refresh token
    if (!Auth::TokenService::GetInstance().Verify(
        refresh_token, Auth::TokenType::Refresh, uid, old_jti))
    {
        co_return Utils::CreateErrorResponse(401, 401, "invalid or expired refresh token");
    }

    if (uid.empty() || old_jti.empty())
        co_return Utils::CreateErrorResponse(401, 401, "invalid refresh token claims");

    // Verify user still exists
    auto check = co_await _user_repo->GetUserProfileByUid(uid);
    if (!check.IsValid())
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
    std::string uid;
    std::string jti;
    // Verify refresh token (ignore failure for logout)
    if (!Auth::TokenService::GetInstance().Verify(
        refresh_token, Auth::TokenType::Refresh, uid, jti))
    {
        auto resp = Utils::CreateSuccessResp(200, 200, "logged out");
        resp->addCookie(BuildClearedRefreshTokenCookie());
        co_return resp;
    }

    // Revoke refresh session if user exists
    if (!uid.empty())
        co_await _redis_service->RevokeRefreshSession(uid);

    auto resp = Utils::CreateSuccessResp(200, 200, "logged out");
    resp->addCookie(BuildClearedRefreshTokenCookie());
    co_return resp;
}