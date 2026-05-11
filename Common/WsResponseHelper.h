#pragma once
#include <json/json.h>
#include "Common/WsProtocol.h"
#include "const.h"

/// WS 协议响应构建器
/// 所有发往客户端的 WebSocket JSON 消息都通过此处构造，避免在各处散落字面量。
namespace WsResponse
{
    /// 心跳确认（type=2）
    inline Json::Value HeartbeatAck(Json::Int64 server_time)
    {
        Json::Value msg;
        msg["type"]        = static_cast<int>(WsMsg::Type::HeartbeatAck);
        msg["server_time"] = server_time;
        return msg;
    }

    /// Token 续期成功（type=4）
    inline Json::Value TokenRefreshed(Json::Int64 expires_at)
    {
        Json::Value msg;
        msg["type"]       = static_cast<int>(WsMsg::Type::TokenRefreshed);
        msg["expires_at"] = expires_at;
        return msg;
    }

    /// Token 续期失败（type=10, sub_type=2）
    inline Json::Value ErrorTokenRefreshFail(const std::string& reason)
    {
        Json::Value msg;
        msg["type"]     = static_cast<int>(WsMsg::Type::Error);
        msg["sub_type"] = static_cast<int>(WsMsg::ErrorSubType::TokenRefreshFail);
        msg["error"]    = reason;
        return msg;
    }

    /// Token 过期提示（type=10, sub_type=1）
    /// expires_in=0 表示已过期，>0 表示剩余秒数
    inline Json::Value ErrorTokenExpiring(Json::Int64 expires_in)
    {
        Json::Value msg;
        msg["type"]       = static_cast<int>(WsMsg::Type::Error);
        msg["sub_type"]   = static_cast<int>(WsMsg::ErrorSubType::TokenExpiring);
        msg["expires_in"] = expires_in;
        return msg;
    }

    /// 通用错误（type=10, sub_type=0）
    inline Json::Value ErrorGeneral(ChatCode::Code code, const std::string& error)
    {
        Json::Value msg;
        msg["type"]     = static_cast<int>(WsMsg::Type::Error);
        msg["sub_type"] = static_cast<int>(WsMsg::ErrorSubType::General);
        msg["code"]     = static_cast<int>(code);
        msg["error"]    = error;
        return msg;
    }

} // namespace WsResponse
