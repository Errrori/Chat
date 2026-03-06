#include "pch.h"
#include "ChatController.h"
#include "Service/ConnectionService.h"
#include "models/AiChats.h"

#include "Container.h"
#include "Common/ChatMessage.h"
#include "Service/MessageService.h"
#include "Common/User.h"
#include "Service/ThreadService.h"

//Message types:
//Image,Text,ErrorMessage,Relationship


void ChatController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& msg,
                                            const drogon::WebSocketMessageType& type)
{
    drogon::async_run([this, conn, msg = std::move(msg)]() mutable -> drogon::Task<>
    {
        try
        {
            if (msg.empty())
                co_return;

            Json::Value msg_data;
            Json::Reader reader;
            if (!reader.parse(msg, msg_data))
            {
                LOG_ERROR << "can not parse message";
                Utils::SendJson(conn, Utils::GenErrorResponse("fail to parse message", ChatCode::InValidJson));
                co_return;
            }

            if ((!msg_data.isMember("content") && !msg_data.isMember("attachment"))
                || !msg_data.isMember("thread_id"))
            {
                LOG_ERROR << "lack of essential field";
                Utils::SendJson(conn, Utils::GenErrorResponse("lack of essential field", ChatCode::MissingField));
                co_return;
            }

            const auto& conn_info = GET_CONN_SERVICE->GetConnInfo(conn);
            if (!conn_info)
            {
                Utils::SendJson(conn, Utils::GenErrorResponse("connection info not found", ChatCode::NotPermission));
                co_return;
            }

            if (!co_await GET_THREAD_SERVICE->ValidateMemberCoro(msg_data["thread_id"].asInt(), conn_info->getUid()))
            {
                LOG_ERROR << "user: " << conn_info->getUid() << " is not in thread: " << msg_data["thread_id"].asInt();
                Utils::SendJson(conn, Utils::GenErrorResponse("user not in thread", ChatCode::NotPermission));
                co_return;
            }

            if (msg_data.isMember("request_data"))
            {
                GET_MESSAGE_SERVICE->ProcessAIRequest(std::move(msg_data), conn);
                co_return;
            }

            auto message = ChatMessage::FromJson(msg_data);

            message.setSenderUid(conn_info->getUid());
            message.setSenderName(conn_info->getUsername());
            message.setSenderAvatar(conn_info->getAvatar());
            GET_MESSAGE_SERVICE->ProcessUserMsg(std::move(message), [conn](const Json::Value& error)
                {
                    if (conn && conn->connected())
                        Utils::SendJson(conn, error);
                    LOG_ERROR << "process message error: " << error.toStyledString();
                });
        }
        catch (const std::exception& e)
        {
            Utils::SendJson(conn, Utils::GenErrorResponse(std::string("system exception: ") + e.what(), ChatCode::SystemException));
        }
    });
}

void ChatController::handleNewConnection(const drogon::HttpRequestPtr& req,
	const drogon::WebSocketConnectionPtr& conn)
{
    auto token = Utils::Authentication::GetToken(req);

    UserInfo info;
    if (!Utils::Authentication::VerifyJWT(token, info))
    {
        conn->send("Failed to verify token");
        conn->shutdown();
        return;
    }

    //after verify,write user info
    const auto& conn_service = GET_CONN_SERVICE;

    const auto username = info.getUsername();
    if (!conn_service->AddConnection(conn,std::move(info)))
    {
        Utils::SendJson(conn, Utils::GenErrorResponse("can not add connection",ChatCode::FailAddConn));
        conn->shutdown();
        LOG_ERROR << "can not add connection!";
    }
    else
    {
        LOG_INFO << "add new connection: " << username;
    }
}

void ChatController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    const auto& info_ptr = conn->getContext<UserInfo>();
    if (info_ptr) {
		LOG_INFO << "username: " << info_ptr->getUsername() << "  connection close";
    }
}