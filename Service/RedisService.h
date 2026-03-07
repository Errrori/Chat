#pragma once
#include <drogon/drogon.h>
#include <drogon/nosql/RedisClient.h>
#include <string>
#include <vector>
#include <optional>

// Redis key 前缀常量
namespace RedisKeys {
    constexpr auto OnlineUser   = "online:{}";        // SET  online:{uid} 1 EX ttl
    constexpr auto UserInfoHash = "user:info:{}";     // HASH user:info:{uid}
    constexpr auto OfflineQueue = "offline:{}";       // LIST offline:{uid}

    // TTL（秒）
    constexpr int OnlineTTL      = 86400;   // 1 天
    constexpr int UserInfoTTL    = 300;     // 5 分钟
    constexpr int OfflineQueueTTL = 604800; // 7 天
}

/**
 * RedisService — 封装所有 Redis 操作。
 *
 * 三大职责：
 *   A. 用户在线状态：SetOnline / SetOffline / IsOnline
 *   B. 用户信息缓存：CacheUserInfo / GetCachedUserInfo / InvalidateUserInfo
 *   C. 离线消息队列：PushOfflineMessage / PopAllOfflineMessages
 */
class RedisService
{
public:
    explicit RedisService(drogon::nosql::RedisClientPtr client)
        : _client(std::move(client)) {}

    RedisService(const RedisService&) = delete;
    RedisService& operator=(const RedisService&) = delete;

    // ──────────────────────────────────────────────
    // A. 在线状态
    // ──────────────────────────────────────────────

    /// 标记用户上线，带 TTL 兜底（防连接未正常关闭导致 key 残留）
    drogon::Task<> SetOnline(const std::string& uid);

    /// 标记用户下线
    drogon::Task<> SetOffline(const std::string& uid);

    /// 查询用户是否在线
    drogon::Task<bool> IsOnline(const std::string& uid);

    // ──────────────────────────────────────────────
    // B. 用户信息缓存（Hash 类型）
    // ──────────────────────────────────────────────

    /// 将 UserInfo JSON 写入 Redis Hash，TTL = UserInfoTTL
    drogon::Task<> CacheUserInfo(const std::string& uid, const Json::Value& user_json);

    /// 从缓存读取用户信息；cache miss 返回 Json::nullValue
    drogon::Task<Json::Value> GetCachedUserInfo(const std::string& uid);

    /// 使缓存失效（用户修改资料时调用）
    drogon::Task<> InvalidateUserInfo(const std::string& uid);

    // ──────────────────────────────────────────────
    // C. 离线消息队列（List 类型）
    // ──────────────────────────────────────────────

    /// 将序列化消息推入离线队列尾部
    drogon::Task<> PushOfflineMessage(const std::string& uid, const std::string& message_json);

    /// 取出并清空该用户所有离线消息（用户上线时调用）
    drogon::Task<std::vector<std::string>> PopAllOfflineMessages(const std::string& uid);

private:
    drogon::nosql::RedisClientPtr _client;

    // 格式化 key：将 {} 替换为 uid
    static std::string MakeKey(const char* pattern, const std::string& uid);
};
