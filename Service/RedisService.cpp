#include "pch.h"
#include "RedisService.h"
#include <json/json.h>

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

drogon::Task<> RedisService::CacheUserInfo(const std::string& uid, const Json::Value& user_json)
{
    try
    {
        auto key = MakeKey(RedisKeys::UserInfoHash, uid);
        // 将 Json::Value 各字段写入 Hash
        for (const auto& field_name : user_json.getMemberNames())
        {
            const auto& val = user_json[field_name];
            std::string str_val = val.isString() ? val.asString() : val.toStyledString();
            // 去掉 toStyledString 末尾的换行
            if (!str_val.empty() && str_val.back() == '\n')
                str_val.pop_back();
            co_await _client->execCommandCoro("HSET %s %s %s",
                key.c_str(), field_name.c_str(), str_val.c_str());
        }
        co_await _client->execCommandCoro("EXPIRE %s %d", key.c_str(), RedisKeys::UserInfoTTL);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::CacheUserInfo error: " << e.what();
    }
}

drogon::Task<Json::Value> RedisService::GetCachedUserInfo(const std::string& uid)
{
    try
    {
        auto key = MakeKey(RedisKeys::UserInfoHash, uid);
        auto result = co_await _client->execCommandCoro("HGETALL %s", key.c_str());

        if (result.isNil() || result.size() == 0)
            co_return Json::nullValue;

        Json::Value json;
        // HGETALL 返回 field1, value1, field2, value2 ...
        for (size_t i = 0; i + 1 < result.size(); i += 2)
        {
            json[result[i].asString()] = result[i + 1].asString();
        }
        co_return json;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::GetCachedUserInfo error: " << e.what();
        co_return Json::nullValue;
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
    try
    {
        auto key = MakeKey(RedisKeys::OfflineQueue, uid);
        co_await _client->execCommandCoro("RPUSH %s %s", key.c_str(), message_json.c_str());
        co_await _client->execCommandCoro("EXPIRE %s %d", key.c_str(), RedisKeys::OfflineQueueTTL);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "RedisService::PushOfflineMessage error: " << e.what();
    }
}

drogon::Task<std::vector<std::string>> RedisService::PopAllOfflineMessages(const std::string& uid)
{
    std::vector<std::string> messages;
    try
    {
        auto key = MakeKey(RedisKeys::OfflineQueue, uid);

        // 先 LRANGE 取全部，再 DEL（原子性通过管道或 Lua 更好，此处简化处理）
        auto result = co_await _client->execCommandCoro("LRANGE %s 0 -1", key.c_str());
        if (!result.isNil())
        {
            for (size_t i = 0; i < result.size(); ++i)
                messages.push_back(result[i].asString());
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
