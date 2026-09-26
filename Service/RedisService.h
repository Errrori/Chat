#pragma once
#include <drogon/drogon.h>
#include <drogon/utils/coroutine.h>
#include <drogon/nosql/RedisClient.h>
#include "Common/User.h"
#include "Common/OutboundMessage.h"
#include <optional>
#include <string>
#include <vector>


// Redis key 前缀常量
namespace RedisKeys {
    constexpr auto OnlineUser    = "online:{}";        // SET  online:{uid} 1 EX ttl
    constexpr auto UserDisplayHash = "user:display:{}"; // HASH user:display:{uid}
    constexpr auto OfflineMessageQueue = "offline:message:{}"; // LIST offline:message:{uid}
    constexpr auto OfflineNoticeQueue  = "offline:notice:{}";  // LIST offline:notice:{uid}
    constexpr auto PendingDeliveryQueue = "pending:delivery:{}"; // LIST pending:delivery:{uid}
    constexpr auto RefreshSession = "auth:refresh:{}"; // STRING auth:refresh:{uid} = jti
    constexpr auto NodeAlive      = "node:alive:{}";   // STRING node:alive:{nodeId} = 1 EX ttl
    constexpr auto UserRoute      = "route:user:{}";   // STRING route:user:{uid} = nodeId

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
 *   B. 展示资料缓存：CacheDisplayProfile / GetCachedDisplayProfile / InvalidateDisplayProfile
 *   C. 离线消息队列：PushOfflinePacket / PopAllOfflineMessages / PopAllOfflineNotices
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
    // B. 展示资料缓存（Hash 类型）
    // ──────────────────────────────────────────────

    /// 将展示资料写入 Redis Hash，TTL = UserInfoTTL
    drogon::Task<> CacheDisplayProfile(const std::string& uid, const UserInfo& profile);

    /// 从缓存读取展示资料；cache miss 返回空 UserInfo
    drogon::Task<UserInfo> GetCachedDisplayProfile(const std::string& uid);

    /// 使缓存失效（用户修改资料时调用）
    drogon::Task<> InvalidateDisplayProfile(const std::string& uid);

    // ──────────────────────────────────────────────
    // C. 离线消息队列（List 类型）
    // ──────────────────────────────────────────────

    /// 按业务通道推入离线包
    drogon::Task<bool> PushOfflinePacket(const std::string& uid, const std::string& packet_json,
                                         ChatDelivery::OfflineChannel channel);

    /// 将离线消息队列原子移动到 pending 队列，避免推送过程中的半删除问题
    drogon::Task<bool> MoveOfflineMessagesToPending(const std::string& uid);

    /// 从 pending 队列逐条弹出离线消息
    drogon::Task<std::optional<std::string>> PopPendingOfflineMessage(const std::string& uid);

    /// 将 pending 中剩余消息原子恢复回 offline 队列
    drogon::Task<> RestorePendingOfflineMessages(const std::string& uid);

    /// 取出并清空该用户所有离线消息（用户上线时调用）
    drogon::Task<std::vector<std::string>> PopAllOfflineMessages(const std::string& uid);

    /// 取出并清空该用户所有离线通知
    drogon::Task<std::vector<std::string>> PopAllOfflineNotices(const std::string& uid);

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

    // ──────────────────────────────────────────────
    // E. 集群节点注册与跨节点消息路由
    //    Key: node:alive:{nodeId}  存在即代表节点存活（带 TTL）
    //         route:user:{uid}     该用户当前连接的节点
    // ──────────────────────────────────────────────

    /// 节点心跳续期，写入 node:alive:{nodeId}，TTL = Cluster::NodeAliveTTL
    drogon::Task<> RegisterNode(const std::string& node_id);

    /// 查询节点是否存活
    drogon::Task<bool> IsNodeAlive(const std::string& node_id);

    /// 删除节点存活标记（优雅退出时调用，令对端立即回退路由）
    drogon::Task<> RemoveNode(const std::string& node_id);

    /// 写入用户路由 uid -> node_id（TTL = Cluster::UserRouteTTL）
    drogon::Task<> SetUserRoute(const std::string& uid, const std::string& node_id);

    /// 查询用户所在节点；无路由返回 nullopt
    drogon::Task<std::optional<std::string>> GetUserRoute(const std::string& uid);

    /// 清除用户路由：仅当当前路由指向 node_id 时才删除（Lua COMPARE-AND-DEL）。
    /// 避免旧节点延迟的断开回调误删新节点刚写入的路由。
    drogon::Task<> ClearUserRoute(const std::string& uid, const std::string& node_id);

    /// 向指定节点的订阅通道发布消息体
    drogon::Task<bool> PublishToNode(const std::string& node_id, const std::string& payload);

    /// 列出当前存活节点 id（SCAN node:alive:*），用于集群可观测性
    drogon::Task<std::vector<std::string>> GetAliveNodes();

    /// 暴露底层客户端，供 ClusterService 创建订阅者
    drogon::nosql::RedisClientPtr GetClient() const { return _client; }

private:
    drogon::nosql::RedisClientPtr _client;

    // 格式化 key：将 {} 替换为 uid
    static std::string MakeKey(const char* pattern, const std::string& uid);
};
