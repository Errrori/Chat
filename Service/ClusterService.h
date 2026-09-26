#pragma once
#include <drogon/nosql/RedisClient.h>
#include <drogon/utils/coroutine.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include "Common/OutboundMessage.h"

class RedisService;

/// ClusterService — 多节点水平扩展的集群通信层。
///
/// 职责：
///   1. 节点生命周期：向 Redis 注册自身存活标记（带 TTL 的心跳）。
///   2. 订阅本节点专属通道 node:channel:{nodeId}，接收其他节点转发来的消息。
///   3. 提供路由能力：查询 uid 所在节点、将消息 PUBLISH 到目标节点通道。
///
/// 收到跨节点消息后，通过 LocalDeliverer 回调交给本进程的 ConnectionService
/// 进行本地投递（本地在线则直发，否则回退离线队列）。
class ClusterService : public std::enable_shared_from_this<ClusterService>
{
public:
    using LocalDeliverFn = std::function<void(const std::string& uid,
                                              const Json::Value& envelope,
                                              ChatDelivery::DeliveryPolicy policy,
                                              ChatDelivery::OfflineChannel channel)>;

    /// 收到其他节点控制消息时的回调（action, target_uid, reason）
    using ControlFn = std::function<void(const std::string& action,
                                         const std::string& target_uid,
                                         const std::string& reason)>;

    ClusterService(std::shared_ptr<RedisService> redis, std::string node_id);

    ClusterService(const ClusterService&) = delete;
    ClusterService& operator=(const ClusterService&) = delete;

    /// 设置本地投递回调（由 ConnectionService 注入）
    void SetLocalDeliverer(LocalDeliverFn fn) { _local_deliverer = std::move(fn); }

    /// 设置控制消息回调（由 ConnectionService 注入）
    void SetControlHandler(ControlFn fn) { _control_handler = std::move(fn); }

    /// 启动：注册节点 + 启动心跳 + 订阅本节点通道。需在事件循环就绪后调用。
    void Start();

    const std::string& NodeId() const { return _node_id; }

    /// 将消息发布到目标节点通道
    drogon::Task<bool> RouteToNode(const std::string& node_id,
                                   const std::string& target_uid,
                                   const ChatDelivery::OutboundMessage& message) const;

    /// 向目标节点发送控制消息（kick/drain/presence），不进入离线队列
    drogon::Task<bool> SendControl(const std::string& node_id,
                                   const std::string& action,
                                   const std::string& target_uid,
                                   const std::string& reason) const;

    /// 查询用户当前所在节点
    drogon::Task<std::optional<std::string>> LocateUser(const std::string& uid) const;

    /// 查询目标节点是否存活
    drogon::Task<bool> IsNodeAlive(const std::string& node_id) const;

private:
    void RegisterSelf();
    void StartHeartbeat();
    void SubscribeSelf();
    void OnClusterMessage(const std::string& channel, const std::string& message);

    std::shared_ptr<RedisService> _redis_service;
    std::string _node_id;
    std::shared_ptr<drogon::nosql::RedisSubscriber> _subscriber;
    LocalDeliverFn _local_deliverer;
    ControlFn _control_handler;
    trantor::TimerId _heartbeat_timer{};
};
