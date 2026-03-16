#include "pch.h"
#include "ChatController.h"
#include "Service/ConnectionService.h"
#include "Service/UserService.h"
#include "models/AiChats.h"
#include "Container.h"
#include "Common/ChatMessage.h"
#include "Service/MessageService.h"
#include "Common/User.h"
#include "Common/ConnectionContext.h"
#include <drogon/utils/coroutine.h>

void ChatController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& msg,
    const drogon::WebSocketMessageType& type)
{
    try
    {
        if (msg.empty())
            return;

        if (type != drogon::WebSocketMessageType::Text)
        {
	        //��ʱ�������ı���Ϣ
			LOG_WARN << "Received non-text message, ignoring.";
            return;
        }
        Json::Value msg_data;
        Json::Reader reader;
        if (!reader.parse(msg, msg_data))
        {
            LOG_ERROR << "can not parse message";
            Utils::SendJson(conn, Utils::GenErrorResponse("fail to parse message", ChatCode::InValidJson));
            return;
        }

        if ((!msg_data.isMember("content") && !msg_data.isMember("attachment"))
            || !msg_data.isMember("thread_id"))
        {
            LOG_ERROR << "lack of essential field";
            Utils::SendJson(conn, Utils::GenErrorResponse("lack of essential field", ChatCode::MissingField));
            return;
        }

        const auto& conn_info = Container::GetInstance().GetConnectionService()->GetConnInfo(conn);
        if (!conn_info)
        {
            Utils::SendJson(conn, Utils::GenErrorResponse("connection info not found", ChatCode::NotPermission));
            return;
        }

        if (msg_data.isMember("request_data"))
        {
            Container::GetInstance().GetMessageService()->ProcessAIRequest(std::move(msg_data), conn);
            return;
        }

        const auto uid = conn_info->uid;
        auto msg_copy = msg_data;
        drogon::async_run(
            [conn, uid, msg_copy = std::move(msg_copy)]() mutable -> drogon::Task<>
            {
                auto display = co_await Container::GetInstance().GetUserService()->GetDisplayProfileByUid(uid);
                if (!display.IsValid())
                {
                    if (conn && conn->connected())
                        Utils::SendJson(conn, Utils::GenErrorResponse("user not found", ChatCode::NotPermission));
                    co_return;
                }

                auto message = ChatMessage::FromJson(msg_copy);
                message.setSenderUid(uid);
                message.setSenderName(display.GetUsername().value_or(""));
                message.setSenderAvatar(display.GetAvatar().value_or(""));

                Container::GetInstance().GetMessageService()->ProcessUserMsg(std::move(message), [conn](const Json::Value& error)
                    {
                        if (conn && conn->connected())
                            Utils::SendJson(conn, error);
                        LOG_ERROR << "process message error: " << error.toStyledString();
                    });
                co_return;
            }
        );
    }
    catch (const std::exception& e)
    {
        Utils::SendJson(conn, Utils::GenErrorResponse(std::string("system exception: ") + e.what(), ChatCode::SystemException));
    }

}

void ChatController::handleNewConnection(const drogon::HttpRequestPtr& req,
    const drogon::WebSocketConnectionPtr& conn)
{
    auto token = Utils::Authentication::GetToken(req);

    std::string uid;
    if (!Utils::Authentication::VerifyJWT(token, uid))
    {
        conn->send("Failed to verify token");
        conn->shutdown();
        return;
    }

    if (uid.empty())
    {
        conn->send("Invalid token claims");
        conn->shutdown();
        return;
    }

    const auto& conn_service = Container::GetInstance().GetConnectionService();
    if (!conn_service->AddConnection(conn, uid))
    {
        Utils::SendJson(conn, Utils::GenErrorResponse("can not add connection", ChatCode::FailAddConn));
        conn->shutdown();
        LOG_ERROR << "can not add connection!";
    }
    else
    {
        LOG_INFO << "add new connection: " << uid;
    }
}


void ChatController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    const auto& info_ptr = conn->getContext<ConnectionContext>();
    if (info_ptr) {
        LOG_INFO << "uid: " << info_ptr->uid << "  connection close";
    }
    Container::GetInstance().GetConnectionService()->RemoveConnection(conn);
}
