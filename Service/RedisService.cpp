#include "pch.h"
#include "RedisService.h"
#include <drogon/utils/coroutine.h>

std::string RedisService::MakeKey(const char* pattern, const std::string& uid)
{
    std::string key(pattern);
    auto pos = key.find("{}");
    if (pos != std::string::npos)
        key.replace(pos, 2, uid);
    return key;
}

drogon::Task<> RedisService::SetOnline(const std::string& uid)
{
    try
    {
        auto key = MakeKey(RedisKeys::OnlineUser, uid);
        co_await _client->execCommandCoro("SET %s 1 EX %d", key.c_str(), RedisKeys::OnlineTTL);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::SetOnline error: " << e.what();
    }
}

drogon::Task<> RedisService::SetOffline(const std::string& uid)
{
    try
    {
        auto key = MakeKey(RedisKeys::OnlineUser, uid);
        co_await _client->execCommandCoro("DEL %s", key.c_str());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::SetOffline error: " << e.what();
    }
}

drogon::Task<bool> RedisService::IsOnline(const std::string& uid)
{
    try
    {
        auto key = MakeKey(RedisKeys::OnlineUser, uid);
        auto result = co_await _client->execCommandCoro("EXISTS %s", key.c_str());
        co_return result.asInteger() > 0;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::IsOnline error: " << e.what();
        co_return false;
    }
}

drogon::Task<> RedisService::CacheDisplayProfile(const std::string& uid, const UsersInfo& profile)
{
    try
    {
        auto key = MakeKey(RedisKeys::UserDisplayHash, uid);
        co_await _client->execCommandCoro("HSET %s username %s", key.c_str(), profile.GetUsername().value_or("").c_str());
        co_await _client->execCommandCoro("HSET %s avatar %s", key.c_str(), profile.GetAvatar().value_or("").c_str());
        co_await _client->execCommandCoro("EXPIRE %s %d", key.c_str(), RedisKeys::UserInfoTTL);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::CacheDisplayProfile error: " << e.what();
    }
}

drogon::Task<UsersInfo> RedisService::GetCachedDisplayProfile(const std::string& uid)
{
    try
    {
        auto key = MakeKey(RedisKeys::UserDisplayHash, uid);
        auto result = co_await _client->execCommandCoro("HGETALL %s", key.c_str());
        if (result.isNil())
            co_return UsersInfo{};

        auto arr = result.asArray();
        if (arr.empty())
            co_return UsersInfo{};

        Json::Value json(Json::objectValue);
        for (size_t i = 0; i + 1 < arr.size(); i += 2)
            json[arr[i].asString()] = arr[i + 1].asString();

        co_return UserInfoBuilder::BuildCached(json);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::GetCachedDisplayProfile error: " << e.what();
        co_return UsersInfo{};
    }
}

drogon::Task<> RedisService::InvalidateDisplayProfile(const std::string& uid)
{
    try
    {
        auto key = MakeKey(RedisKeys::UserDisplayHash, uid);
        co_await _client->execCommandCoro("DEL %s", key.c_str());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::InvalidateDisplayProfile error: " << e.what();
    }
}

drogon::Task<> RedisService::PushOfflinePacket(const std::string& uid,
                                               const std::string& packet_json,
                                               ChatDelivery::OfflineChannel channel)
{

    try
    {
        if (channel == ChatDelivery::OfflineChannel::Notice)
        {
            auto key = MakeKey(RedisKeys::OfflineNoticeQueue, uid);
            co_await _client->execCommandCoro("RPUSH %s %s", key.c_str(), packet_json.c_str());
            co_await _client->execCommandCoro("EXPIRE %s %d", key.c_str(), RedisKeys::OfflineQueueTTL);
        }
        else
        {
            auto key = MakeKey(RedisKeys::OfflineMessageQueue, uid);
            co_await _client->execCommandCoro("RPUSH %s %s", key.c_str(), packet_json.c_str());
            co_await _client->execCommandCoro("EXPIRE %s %d", key.c_str(), RedisKeys::OfflineQueueTTL);
        }
    }catch (const std::exception& e)
    {
        LOG_ERROR << "Redis push offline packet: " << e.what();
    }
    
}

drogon::Task<std::vector<std::string>> RedisService::PopAllOfflineMessages(const std::string& uid)
{
    std::vector<std::string> messages;
    try
    {
        auto key = MakeKey(RedisKeys::OfflineMessageQueue, uid);
        auto result = co_await _client->execCommandCoro("LRANGE %s 0 -1", key.c_str());
        if (!result.isNil())
        {
            for (const auto& item : result.asArray())
                messages.push_back(item.asString());
        }

        if (!messages.empty())
            co_await _client->execCommandCoro("DEL %s", key.c_str());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::PopAllOfflineMessages error: " << e.what();
    }
    co_return messages;
}

drogon::Task<std::vector<std::string>> RedisService::PopAllOfflineNotices(const std::string& uid)
{
    std::vector<std::string> notices;
    try
    {
        auto key = MakeKey(RedisKeys::OfflineNoticeQueue, uid);
        auto result = co_await _client->execCommandCoro("LRANGE %s 0 -1", key.c_str());
        if (!result.isNil())
        {
            for (const auto& item : result.asArray())
                notices.push_back(item.asString());
        }

        if (!notices.empty())
            co_await _client->execCommandCoro("DEL %s", key.c_str());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::PopAllOfflineNotices error: " << e.what();
    }
    co_return notices;
}

drogon::Task<> RedisService::StoreRefreshSession(const std::string& uid,
                                                const std::string& jti,
                                                int ttl_seconds)
{
    try
    {
        auto key = MakeKey(RedisKeys::RefreshSession, uid);
        co_await _client->execCommandCoro("SET %s %s EX %d",
                                          key.c_str(), jti.c_str(), ttl_seconds);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::StoreRefreshSession error: " << e.what();
    }
}

drogon::Task<bool> RedisService::ValidateRefreshSession(const std::string& uid,
                                                       const std::string& jti)
{
    try
    {
        auto key = MakeKey(RedisKeys::RefreshSession, uid);
        auto result = co_await _client->execCommandCoro("GET %s", key.c_str());
        if (result.isNil())
            co_return false;
        co_return result.asString() == jti;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::ValidateRefreshSession error: " << e.what();
        co_return false;
    }
}

drogon::Task<bool> RedisService::RotateRefreshSession(const std::string& uid,
                                                     const std::string& old_jti,
                                                     const std::string& new_jti,
                                                     int ttl_seconds)
{
    try
    {
        auto key = MakeKey(RedisKeys::RefreshSession, uid);
        auto stored = co_await _client->execCommandCoro("GET %s", key.c_str());
        if (stored.isNil() || stored.asString() != old_jti)
        {
            LOG_WARN << "[RedisService] RotateRefreshSession: jti mismatch for uid=" << uid;
            co_return false;
        }

        co_await _client->execCommandCoro("SET %s %s EX %d",
                                          key.c_str(), new_jti.c_str(), ttl_seconds);
        co_return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::RotateRefreshSession error: " << e.what();
        co_return false;
    }
}

drogon::Task<> RedisService::RevokeRefreshSession(const std::string& uid)
{
    try
    {
        auto key = MakeKey(RedisKeys::RefreshSession, uid);
        co_await _client->execCommandCoro("DEL %s", key.c_str());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::RevokeRefreshSession error: " << e.what();
    }
}
