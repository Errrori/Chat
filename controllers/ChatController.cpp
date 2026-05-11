#include "pch.h"
#include "Common/ResponseHelper.h"
#include "Common/WsResponseHelper.h"
#include "ChatController.h"
#include "Service/ConnectionService.h"
#include "Service/UserService.h"
#include "models/AiChats.h"
#include "Container.h"
#include "Service/MessageService.h"
#include "Common/ConnectionContext.h"
#include "Common/HeartbeatConfig.h"
#include "Common/WsProtocol.h"
#include <drogon/utils/coroutine.h>

#include "auth/TokenService.h"

/// 根据消息整数 type 字段路由，不保留旧字符串格式兼容
static WsMsg::Type ParseMessageType(const Json::Value& msg)
{
    if (msg.isMember("type") && msg["type"].isInt())
    {
        const int t = msg["type"].asInt();
        switch (t)
        {
        case static_cast<int>(WsMsg::Type::Heartbeat):    return WsMsg::Type::Heartbeat;
        case static_cast<int>(WsMsg::Type::TokenRefresh): return WsMsg::Type::TokenRefresh;
        case static_cast<int>(WsMsg::Type::ChatSend):     return WsMsg::Type::ChatSend;
        case static_cast<int>(WsMsg::Type::AiRequest):    return WsMsg::Type::AiRequest;
        default: break;
        }
    }
    return WsMsg::Type::Error; // 无法识别
}

// ─────────────────────────────────────────────────────────────────────────────

/// 心跳：无论 token 是否过期都立即响应，让客户端确认连接仍然存活
static void HandleHeartbeat(const drogon::WebSocketConnectionPtr& conn)
{
    Utils::SendJson(conn, WsResponse::HeartbeatAck(
        static_cast<Json::Int64>(Utils::GetCurrentTimeStamp())));
}

/// Token 续期：在宽限期内接受新 access_token，更新连接状态
static void HandleTokenRefresh(const drogon::WebSocketConnectionPtr& conn,
    const Json::Value& msg_data,
    const ConnectionStateSnapshot& snapshot)
{
    const auto now = std::chrono::system_clock::now();
    const auto grace_deadline = snapshot.expiry
        + std::chrono::duration<double>(Heartbeat::RefreshGracePeriodSec);

    if (now > grace_deadline)
    {
        conn->shutdown(drogon::CloseCode::kViolation, "token expired beyond grace period");
        return;
    }

    if (!msg_data.isMember("access_token") || msg_data["access_token"].asString().empty())
    {
        Utils::SendJson(conn, WsResponse::ErrorTokenRefreshFail("missing access_token field"));
        return;
    }

    const auto& conn_service = Container::GetInstance().GetConnectionService();
    if (conn_service->RefreshConnectionToken(conn, msg_data["access_token"].asString()))
    {
        Json::Int64 new_expiry = 0;
        if (const auto refreshed = conn_service->GetConnectionSnapshot(conn))
        {
            new_expiry = static_cast<Json::Int64>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    refreshed->expiry.time_since_epoch()).count());
        }
        Utils::SendJson(conn, WsResponse::TokenRefreshed(new_expiry));
    }
    else
    {
        Utils::SendJson(conn, WsResponse::ErrorTokenRefreshFail("invalid or mismatched access token"));
    }
}

/// AI 请求：转发给 MessageService 处理
static void HandleAiRequest(const drogon::WebSocketConnectionPtr& conn,
    Json::Value msg_data)
{
    if (!msg_data.isMember("thread_id"))
    {
        Utils::SendJson(conn, ResponseHelper::MakeErrorJson("lack of thread id", ChatCode::MissingField));
        return;
    }
    Container::GetInstance().GetMessageService()->ProcessAIRequest(std::move(msg_data), conn);
}

/// 普通聊天消息：验证内容后异步投递
static void HandleChatSend(const drogon::WebSocketConnectionPtr& conn,
    Json::Value msg_data,
    const ConnectionStateSnapshot& snapshot)
{
    if (!msg_data.isMember("thread_id"))
    {
        Utils::SendJson(conn, ResponseHelper::MakeErrorJson("lack of thread id", ChatCode::MissingField));
        return;
    }

    std::optional<std::string> content;
    std::optional<Json::Value> attachment;
    if (msg_data.isMember("content"))
    {
        const auto& c = msg_data["content"].asString();
        if (!c.empty()) content = c;
    }
    if (msg_data.isMember("attachment"))
    {
        const auto& a = msg_data["attachment"];
        if (!a.isNull()) attachment = a;
    }

    if (!content && !attachment)
    {
        Utils::SendJson(conn, ResponseHelper::MakeErrorJson(
            "can not send message without content and attachment", ChatCode::MissingField));
        return;
    }

    const int   thread_id = msg_data["thread_id"].asInt();
    std::string uid       = snapshot.uid;

    try
    {
        drogon::async_run(
            [thread_id, uid = std::move(uid),
             content = std::move(content), attachment = std::move(attachment)]
            () mutable -> drogon::Task<>
            {
                auto result = co_await Container::GetInstance()
                    .GetMessageService()->ProcessChatMsg(thread_id, uid, content, attachment);
                if (!result.success)
                {
                    LOG_ERROR << "ProcessChatMsg failed, thread_id=" << thread_id
                        << ", error=" << result.error;
                    co_return;
                }
                if (result.partial_degraded)
                {
                    LOG_ERROR << "ProcessChatMsg degraded offline queueing, thread_id="
                        << thread_id << ", redis_failed_targets=" << result.redis_failed_targets;
                }
            }
        );
    }
    catch (const std::exception& e)
    {
        Utils::SendJson(conn, ResponseHelper::MakeErrorJson(
            std::string("can not send message ") + e.what(), ChatCode::InvalidArg));
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void ChatController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& msg,
                                      const drogon::WebSocketMessageType& type)
{
    try
    {
        if (msg.empty())
            return;

        if (type != drogon::WebSocketMessageType::Text)
        {
            LOG_WARN << "Received non-text message, ignoring.";
            return;
        }

        const auto& conn_service = Container::GetInstance().GetConnectionService();
        const auto conn_snapshot = conn_service->GetConnectionSnapshot(conn);
        if (!conn_snapshot)
        {
            Utils::SendJson(conn, ResponseHelper::MakeErrorJson(
                "connection info not found", ChatCode::NotPermission));
            return;
        }

        // 任何客户端文本消息都视为一次活跃触达
        conn_service->TouchConnection(conn);

        Json::Value msg_data;
        Json::Reader reader;
        if (!reader.parse(msg, msg_data))
        {
            LOG_ERROR << "can not parse message";
            Utils::SendJson(conn, ResponseHelper::MakeErrorJson(
                "fail to parse message", ChatCode::InValidJson));
            return;
        }

        const auto msg_type = ParseMessageType(msg_data);

        // 控制消息：不受 token 有效期限制
        if (msg_type == WsMsg::Type::Heartbeat)
        {
            HandleHeartbeat(conn);
            return;
        }
        if (msg_type == WsMsg::Type::TokenRefresh)
        {
            HandleTokenRefresh(conn, msg_data, *conn_snapshot);
            return;
        }

        // 业务消息：token 必须在有效期内
        if (conn_snapshot->expiry < std::chrono::system_clock::now())
        {
            Utils::SendJson(conn, WsResponse::ErrorTokenExpiring(0));
            return;
        }

        switch (msg_type)
        {
        case WsMsg::Type::AiRequest:
            HandleAiRequest(conn, std::move(msg_data));
            break;
        case WsMsg::Type::ChatSend:
            HandleChatSend(conn, std::move(msg_data), *conn_snapshot);
            break;
        default:
            Utils::SendJson(conn, ResponseHelper::MakeErrorJson(
                "unknown message type", ChatCode::InvalidArg));
            break;
        }
    }
    catch (const std::exception& e)
    {
        Utils::SendJson(conn, ResponseHelper::MakeErrorJson(
            std::string("system exception: ") + e.what(), ChatCode::SystemException));
    }
}

void ChatController::handleNewConnection(const drogon::HttpRequestPtr& req,
    const drogon::WebSocketConnectionPtr& conn)
{
    auto token = Utils::Authentication::GetToken(req);

    auto result = Auth::TokenService::GetInstance().
		Verify(token, Auth::TokenType::Access);
    if (!result)
    {
        Utils::SendJson(conn, ResponseHelper::MakeErrorJson("can not add connection", ChatCode::FailAddConn));
        conn->shutdown();
        return;
    }

    const auto& conn_service = Container::GetInstance().GetConnectionService();
    if (!conn_service->AddConnection(conn, result->uid,result->expire_at))
    {
        Utils::SendJson(conn, ResponseHelper::MakeErrorJson("can not add connection", ChatCode::FailAddConn));
		//shut down conn on AddConnection temporarily
    }
}


void ChatController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    LOG_INFO << "connection closed";
    if (conn->disconnected())
        LOG_INFO << "connection is disconnected";
    if (!conn->connected())
        LOG_INFO << "connection is not connected";
	Container::GetInstance().GetConnectionService()->RemoveConnection(conn);
}
