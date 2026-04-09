#include "pch.h"
#include "Common/ResponseHelper.h"
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

        const auto& conn_info = Container::GetInstance().GetConnectionService()->GetConnInfo(conn);
        if (!conn_info)
        {
            Utils::SendJson(conn, ResponseHelper::MakeErrorJson("connection info not found", ChatCode::NotPermission));
            return;
        }

        // 更新最后活跃时间（任何客户端消息都算活跃）
        conn_info->last_active_time = std::chrono::system_clock::now();

        Json::Value msg_data;
        Json::Reader reader;
        if (!reader.parse(msg, msg_data))
        {
            LOG_ERROR << "can not parse message";
            Utils::SendJson(conn, ResponseHelper::MakeErrorJson("fail to parse message", ChatCode::InValidJson));
            return;
        }

        // ── 控制消息分发 ──
        if (msg_data.isMember("type"))
        {
            const auto msg_type = msg_data["type"].asString();

            // 心跳：始终响应（即使过期，也让客户端知道连接还活着）
            if (msg_type == Heartbeat::MsgType::Heartbeat)
            {
                Json::Value ack;
                ack["type"] = Heartbeat::MsgType::HeartbeatAck;
                ack["server_time"] = static_cast<Json::Int64>(Utils::GetCurrentTimeStamp());
                Utils::SendJson(conn, ack);
                return;
            }

            // Token 续期：在宽限期内允许
            if (msg_type == Heartbeat::MsgType::TokenRefresh)
            {
                const auto now = std::chrono::system_clock::now();
                const auto grace_deadline = conn_info->expiry
                    + std::chrono::duration<double>(Heartbeat::RefreshGracePeriodSec);

                if (now > grace_deadline)
                {
                    conn->shutdown(drogon::CloseCode::kViolation,
                        "token expired beyond grace period");
                    return;
                }

                if (!msg_data.isMember("access_token") || msg_data["access_token"].asString().empty())
                {
                    Json::Value fail;
                    fail["type"] = Heartbeat::MsgType::TokenRefreshFailed;
                    fail["error"] = "missing access_token field";
                    Utils::SendJson(conn, fail);
                    return;
                }

                const auto& conn_service = Container::GetInstance().GetConnectionService();
                if (conn_service->RefreshConnectionToken(conn, msg_data["access_token"].asString()))
                {
                    auto new_ctx = conn->getContext<ConnectionContext>();
                    Json::Value ok;
                    ok["type"] = Heartbeat::MsgType::TokenRefreshed;
                    ok["new_expiry"] = static_cast<Json::Int64>(
                        std::chrono::duration_cast<std::chrono::seconds>(
                            new_ctx->expiry.time_since_epoch()).count());
                    Utils::SendJson(conn, ok);
                }
                else
                {
                    Json::Value fail;
                    fail["type"] = Heartbeat::MsgType::TokenRefreshFailed;
                    fail["error"] = "invalid or mismatched access token";
                    Utils::SendJson(conn, fail);
                }
                return;
            }
        }

        // ── 以下为业务消息，必须在有效期内 ──
        if (conn_info->expiry < std::chrono::system_clock::now())
        {
            Json::Value expired_hint;
            expired_hint["type"] = Heartbeat::MsgType::TokenExpiring;
            expired_hint["expires_in"] = 0;
            expired_hint["message"] = "access token expired, please send token_refresh";
            Utils::SendJson(conn, expired_hint);
            return;
        }

        if (!msg_data.isMember("thread_id"))
        {
            Utils::SendJson(conn, ResponseHelper::MakeErrorJson("lack of thread id", ChatCode::MissingField));
            return;
        }

        std::optional<std::string> content;
        std::optional<Json::Value> attachment;
        if (msg_data.isMember("content"))
        {
			const auto& msg_content = msg_data["content"].asString();
	        if (!msg_content.empty())
				content = msg_content;
        }
        if (msg_data.isMember("attachment"))
        {
        	const auto& msg_attachment = msg_data["attachment"];
	        if (!msg_attachment.isNull())
				attachment = msg_attachment;
        }

        if (!content && !attachment)
        {
            Utils::SendJson(conn, ResponseHelper::MakeErrorJson("can not send message without content and attachment",ChatCode::MissingField));
            return;
        }

		int thread_id = msg_data["thread_id"].asInt();


        auto uid = conn_info->uid;

        if (msg_data.isMember("request_data"))
        {
            Container::GetInstance().GetMessageService()->ProcessAIRequest(std::move(msg_data), conn);
            return;
        }

		try
        {
            drogon::async_run(
                [thread_id ,uid = std::move(uid), content = std::move(content), attachment = std::move(attachment)]
					() mutable -> drogon::Task<>
                {
                    auto result = co_await Container::GetInstance().
                		GetMessageService()->ProcessChatMsg(thread_id, uid, content, attachment);
                    if (!result.success)
                    {
                        LOG_ERROR << "ProcessChatMsg failed, thread_id=" << thread_id
                            << ", error=" << result.error;
                        co_return;
                    }

                    if (result.partial_degraded)
                    {
                        LOG_ERROR << "ProcessChatMsg completed with degraded offline queueing, thread_id="
                            << thread_id << ", redis_failed_targets=" << result.redis_failed_targets;
                    }
                }
            );
        }catch (const std::exception& e)
        {
            Utils::SendJson(conn, ResponseHelper::MakeErrorJson(std::string("can not send message ")+ e.what(), ChatCode::InvalidArg));
        }
    }
    catch (const std::exception& e)
    {
        Utils::SendJson(conn, ResponseHelper::MakeErrorJson(std::string("system exception: ") + e.what(), ChatCode::SystemException));
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
