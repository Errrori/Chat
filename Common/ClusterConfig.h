#pragma once
#include <string>

/// 多节点集群配置常量 —— 水平扩展路由参数
namespace Cluster
{
    /// 节点存活标记 TTL（秒）。节点每 NodeHeartbeatSec 秒续期一次，
    /// 超过该时间未续期即视为宕机，路由到它的消息回退到离线队列。
    constexpr int NodeAliveTTL = 30;

    /// 节点心跳间隔（秒）
    constexpr int NodeHeartbeatSec = 10;

    /// 用户路由表 TTL（秒）。用户连上某节点时写入 uid -> nodeId，
    /// 断开时删除；配合节点存活校验可容忍节点崩溃后的脏路由。
    constexpr int UserRouteTTL = 86400;

    /// 每个节点专属订阅通道名：node:channel:{nodeId}
    inline std::string Channel(const std::string& node_id)
    {
        return "node:channel:" + node_id;
    }
} // namespace Cluster
