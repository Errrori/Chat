#pragma once
#include <json/json.h>
#include "Common/HeartbeatConfig.h"

/// WS 协议响应构建器
/// 所有发往客户端的 WebSocket JSON 消息都通过此处构造，避免在各处散落字面量。
namespace WsResponse
{
    /// 心跳确认（heartbeat_ack）
    inline Json::Value HeartbeatAck(Json::Int64 server_time)
    {
        Json::Value msg;
        msg["type"]        = Heartbeat::MsgType::HeartbeatAck;
        msg["server_time"] = server_time;
        return msg;
    }

    /// Token 刷新成功（token_refreshed）
    inline Json::Value TokenRefreshed(Json::Int64 new_expiry_epoch_sec)
    {
        Json::Value msg;
        msg["type"]       = Heartbeat::MsgType::TokenRefreshed;
        msg["new_expiry"] = new_expiry_epoch_sec;
        return msg;
    }

    /// Token 刷新失败（token_refresh_failed）
    inline Json::Value TokenRefreshFailed(const std::string& reason)
    {
        Json::Value msg;
        msg["type"]  = Heartbeat::MsgType::TokenRefreshFailed;
        msg["error"] = reason;
        return msg;
    }

    /// Token 已过期提示（token_expiring，expires_in=0 表示立即过期）
    inline Json::Value TokenExpired()
    {
        Json::Value msg;
        msg["type"]       = Heartbeat::MsgType::TokenExpiring;
        msg["expires_in"] = 0;
        msg["message"]    = "access token expired, please send token_refresh";
        return msg;
    }

} // namespace WsResponse
