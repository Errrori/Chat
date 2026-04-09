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
#include <drogon/utils/coroutine.h>

#include "auth/TokenService.h"

// ─────── Phase 2: 统一消息类型枚举 ──────────────────────────────────────────

/// WS 入站消息类型
enum class WsMessageType
{
    Heartbeat,    ///< 心跳探活（"heartbeat"）
    TokenRefresh, ///< Token 续期（"token_refresh"）
    ChatSend,     ///< 普通聊天消息（type="chat_send" 或老格式 thread_id）
    AiRequest,    ///< AI 补全请求（type="ai_request" 或老格式 request_data）
    Unknown,      ///< 无法识别，将被拒绝
};

/// 根据消息字段推断消息类型，兼容新旧两种格式：
///   新格式：发送方显式带 "type" 字段
///   旧格式：通过 "request_data" / "thread_id" 字段隐式推断
static WsMessageType ParseMessageType(const Json::Value& msg)
{
    if (msg.isMember("type"))
    {
        const auto& t = msg["type"].asString();
        if (t == Heartbeat::MsgType::Heartbeat)    return WsMessageType::Heartbeat;
        if (t == Heartbeat::MsgType::TokenRefresh) return WsMessageType::TokenRefresh;
        if (t == "chat_send")                      return WsMessageType::ChatSend;
        if (t == "ai_request")                     return WsMessageType::AiRequest;
    }
    // ── 向后兼容：无 type 字段时根据 payload 字段推断 ──
    if (msg.isMember("request_data")) return WsMessageType::AiRequest;
    if (msg.isMember("thread_id"))    return WsMessageType::ChatSend;
    return WsMessageType::Unknown;
}

// ─────── Phase 1: 拆分后的消息处理函数 ──────────────────────────────────────

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
        Utils::SendJson(conn, WsResponse::TokenRefreshFailed("missing access_token field"));
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
        Utils::SendJson(conn, WsResponse::TokenRefreshFailed("invalid or mismatched access token"));
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

        // ── Phase 2: 统一路由分发 ──
        const auto msg_type = ParseMessageType(msg_data);

        // 控制消息：不受 token 有效期限制
        if (msg_type == WsMessageType::Heartbeat)
        {
            HandleHeartbeat(conn);
            return;
        }
        if (msg_type == WsMessageType::TokenRefresh)
        {
            HandleTokenRefresh(conn, msg_data, *conn_snapshot);
            return;
        }

        // 业务消息：token 必须在有效期内
        if (conn_snapshot->expiry < std::chrono::system_clock::now())
        {
            Utils::SendJson(conn, WsResponse::TokenExpired());
            return;
        }

        switch (msg_type)
        {
        case WsMessageType::AiRequest:
            HandleAiRequest(conn, std::move(msg_data));
            break;
        case WsMessageType::ChatSend:
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
