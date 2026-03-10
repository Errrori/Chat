#pragma once
#include <drogon/drogon.h>
#include <drogon/nosql/RedisClient.h>
#include <string>
#include <vector>


// Redis key 前缀常量
namespace RedisKeys {
    constexpr auto OnlineUser    = "online:{}";        // SET  online:{uid} 1 EX ttl
    constexpr auto UserInfoHash  = "user:info:{}";     // HASH user:info:{uid}
    constexpr auto OfflineQueue  = "offline:{}";       // LIST offline:{uid}
    constexpr auto RefreshSession = "auth:refresh:{}"; // STRING auth:refresh:{uid} = jti

    // TTL（秒）
    constexpr int OnlineTTL       = 86400;   // 1 天
    constexpr int UserInfoTTL     = 300;     // 5 分钟
    constexpr int OfflineQueueTTL = 604800;  // 7 天
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

    // ──────────────────────────────────────────────
    // D. Refresh Token 会话（单端登录 + 轮换策略）
    //    Key: auth:refresh:{uid}  Value: jti 字符串
    //    单端语义：新登录直接覆盖旧 session
    // ──────────────────────────────────────────────

    /// 登录时写入 refresh session（覆盖旧session，实现单端登录）
    drogon::Task<> StoreRefreshSession(const std::string& uid, const std::string& jti, int ttl_seconds);

    /// 校验传入的 jti 是否与已存储的一致
    drogon::Task<bool> ValidateRefreshSession(const std::string& uid, const std::string& jti);

    /// 轮换：校验旧 jti，成功则原子写入新 jti。失败返回 false
    drogon::Task<bool> RotateRefreshSession(const std::string& uid, const std::string& old_jti,
                                            const std::string& new_jti, int ttl_seconds);

    /// 登出：删除当前设备 refresh session
    drogon::Task<> RevokeRefreshSession(const std::string& uid);

private:
    drogon::nosql::RedisClientPtr _client;

    // 格式化 key：将 {} 替换为 uid
    static std::string MakeKey(const char* pattern, const std::string& uid);
};
