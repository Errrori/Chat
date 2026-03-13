#include "pch.h"
#include "RedisService.h"

namespace
{
    constexpr const char* kFieldUid = "uid";
    constexpr const char* kFieldUsername = "username";
    constexpr const char* kFieldAccount = "account";
    constexpr const char* kFieldAvatar = "avatar";
}

// ──────────────────────────────────────────────
// 私有工具
// ──────────────────────────────────────────────

std::string RedisService::MakeKey(const char* pattern, const std::string& uid)
{
    std::string key(pattern);
    auto pos = key.find("{}");
    if (pos != std::string::npos)
        key.replace(pos, 2, uid);
    return key;
}

const char* RedisService::GetOfflineQueuePattern(ChatDelivery::OfflineChannel channel)
{
    switch (channel)
    {
    case ChatDelivery::OfflineChannel::Notice:
        return RedisKeys::OfflineQueueNotice;
    case ChatDelivery::OfflineChannel::Message:
    default:
        return RedisKeys::OfflineQueueMessage;
    }
}

// ──────────────────────────────────────────────
// A. 在线状态
// ──────────────────────────────────────────────

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

// ──────────────────────────────────────────────
// B. 用户信息缓存
// ──────────────────────────────────────────────

drogon::Task<> RedisService::CacheUserInfo(const std::string& uid, const UserInfo& user_info)
{
    try
    {
        auto key = MakeKey(RedisKeys::UserInfoHash, uid);

        co_await _client->execCommandCoro("HSET %s %s %s",
            key.c_str(), kFieldUid, user_info.getUid().c_str());
        co_await _client->execCommandCoro("HSET %s %s %s",
            key.c_str(), kFieldUsername, user_info.getUsername().c_str());
        co_await _client->execCommandCoro("HSET %s %s %s",
            key.c_str(), kFieldAccount, user_info.getAccount().c_str());
        co_await _client->execCommandCoro("HSET %s %s %s",
            key.c_str(), kFieldAvatar, user_info.getAvatar().c_str());

        co_await _client->execCommandCoro("EXPIRE %s %d", key.c_str(), RedisKeys::UserInfoTTL);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::CacheUserInfo error: " << e.what();
    }
}

drogon::Task<UserInfo> RedisService::GetCachedUserInfo(const std::string& uid)
{
    try
    {
        auto key = MakeKey(RedisKeys::UserInfoHash, uid);
        auto result = co_await _client->execCommandCoro("HGETALL %s", key.c_str());

        if (result.isNil())
            co_return UserInfo{};

        auto arr = result.asArray();
        if (arr.empty())
            co_return UserInfo{};

        std::unordered_map<std::string, std::string> fields;
        // HGETALL 返回 field1, value1, field2, value2 ...
        for (size_t i = 0; i + 1 < arr.size(); i += 2)
        {
            fields[arr[i].asString()] = arr[i + 1].asString();
        }

        UserInfo info;
        if (auto it = fields.find(kFieldUid); it != fields.end())
            info.setUid(it->second);
        if (auto it = fields.find(kFieldUsername); it != fields.end())
            info.setUsername(it->second);
        if (auto it = fields.find(kFieldAccount); it != fields.end())
            info.setAccount(it->second);
        if (auto it = fields.find(kFieldAvatar); it != fields.end())
            info.setAvatar(it->second);

        if (info.getUid().empty())
            co_return UserInfo{};

        co_return info;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::GetCachedUserInfo error: " << e.what();
        co_return UserInfo{};
    }
}

drogon::Task<> RedisService::InvalidateUserInfo(const std::string& uid)
{
    try
    {
        auto key = MakeKey(RedisKeys::UserInfoHash, uid);
        co_await _client->execCommandCoro("DEL %s", key.c_str());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::InvalidateUserInfo error: " << e.what();
    }
}

// ──────────────────────────────────────────────
// C. 离线消息队列
// ──────────────────────────────────────────────

drogon::Task<> RedisService::PushOfflineMessage(const std::string& uid, const std::string& message_json)
{
    co_await PushOfflinePacket(uid, message_json, ChatDelivery::OfflineChannel::Message);
}

drogon::Task<> RedisService::PushOfflineNotice(const std::string& uid, const std::string& notice_json)
{
    co_await PushOfflinePacket(uid, notice_json, ChatDelivery::OfflineChannel::Notice);
}

drogon::Task<> RedisService::PushOfflinePacket(const std::string& uid,
                                               const std::string& packet_json,
                                               ChatDelivery::OfflineChannel channel)
{
    try
    {
        auto key = MakeKey(GetOfflineQueuePattern(channel), uid);
        co_await _client->execCommandCoro("RPUSH %s %s", key.c_str(), packet_json.c_str());
        co_await _client->execCommandCoro("EXPIRE %s %d", key.c_str(), RedisKeys::OfflineQueueTTL);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::PushOfflinePacket error: " << e.what();
    }
}

drogon::Task<std::vector<std::string>> RedisService::PopAllOfflineMessages(const std::string& uid)
{
    auto messages = co_await PopAllOfflinePackets(uid, ChatDelivery::OfflineChannel::Message);

    try
    {
        auto legacy_key = MakeKey(RedisKeys::OfflineQueueLegacy, uid);
        auto legacy_result = co_await _client->execCommandCoro("LRANGE %s 0 -1", legacy_key.c_str());
        if (!legacy_result.isNil())
        {
            for (const auto& item : legacy_result.asArray())
                messages.push_back(item.asString());
        }

        if (!messages.empty())
            co_await _client->execCommandCoro("DEL %s", legacy_key.c_str());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::PopAllOfflineMessages legacy fallback error: " << e.what();
    }

    co_return messages;
}

drogon::Task<std::vector<std::string>> RedisService::PopAllOfflineNotices(const std::string& uid)
{
    co_return co_await PopAllOfflinePackets(uid, ChatDelivery::OfflineChannel::Notice);
}

drogon::Task<std::vector<std::string>> RedisService::PopAllOfflinePackets(
    const std::string& uid, ChatDelivery::OfflineChannel channel)
{
    std::vector<std::string> messages;
    try
    {
        auto key = MakeKey(GetOfflineQueuePattern(channel), uid);

        // 先 LRANGE 取全部，再 DEL（原子性通过管道或 Lua 更好，此处简化处理）
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
        LOG_ERROR << "RedisService::PopAllOfflinePackets error: " << e.what();
    }
    co_return messages;
}

// ──────────────────────────────────────────────
// D. Refresh Token 会话（单端 + 轮换）
// ──────────────────────────────────────────────

drogon::Task<> RedisService::StoreRefreshSession(const std::string& uid,
                                                  const std::string& jti,
                                                  int ttl_seconds)
{
    try
    {
        auto key = MakeKey(RedisKeys::RefreshSession, uid);
        // SET key jti EX ttl — 覆盖旧 session（单端策略）
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
        auto key    = MakeKey(RedisKeys::RefreshSession, uid);
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

        // 校验旧 jti
        auto stored = co_await _client->execCommandCoro("GET %s", key.c_str());
        if (stored.isNil() || stored.asString() != old_jti)
        {
            LOG_WARN << "[RedisService] RotateRefreshSession: jti mismatch for uid=" << uid;
            co_return false;
        }

        // 写入新 jti（覆盖）
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
