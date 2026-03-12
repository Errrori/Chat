#include "pch.h"
#include "ChatController.h"
#include "Service/ConnectionService.h"
#include "Service/UserService.h"
#include "models/AiChats.h"
#include "Container.h"
#include "Common/ChatMessage.h"
#include "Service/MessageService.h"
#include "Common/User.h"

void ChatController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& msg,
    const drogon::WebSocketMessageType& type)
{
    try
    {
        if (msg.empty())
            return;

        if (type != drogon::WebSocketMessageType::Text)
        {
	        //暂时跳过非文本消息
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

        auto message = ChatMessage::FromJson(msg_data);

        message.setSenderUid(conn_info->getUid());
        message.setSenderName(conn_info->getUsername());
        message.setSenderAvatar(conn_info->getAvatar());
        Container::GetInstance().GetMessageService()->ProcessUserMsg(std::move(message), [conn](const Json::Value& error)
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

    // Token carries uid only; resolve full profile from cache/DB before storing.
    drogon::async_run(
        [conn, uid = info.getUid()]() mutable -> drogon::Task<>
        {
            auto full_info = co_await Container::GetInstance().GetUserService()->GetUserInfo(uid);
            if (full_info.getUid().empty())
            {
                Utils::SendJson(conn, Utils::GenErrorResponse("user not found", ChatCode::NotPermission));
                conn->shutdown();
                co_return;
            }

            const auto& conn_service = Container::GetInstance().GetConnectionService();
            const auto username = full_info.getUsername();
            if (!conn_service->AddConnection(conn, std::move(full_info)))
            {
                Utils::SendJson(conn, Utils::GenErrorResponse("can not add connection", ChatCode::FailAddConn));
                conn->shutdown();
                LOG_ERROR << "can not add connection!";
            }
            else
            {
                LOG_INFO << "add new connection: " << username;
            }
        }
    );
}


void ChatController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    const auto& info_ptr = conn->getContext<UserInfo>();
    if (info_ptr) {
        LOG_INFO << "username: " << info_ptr->getUsername() << "  connection close";
    }
    Container::GetInstance().GetConnectionService()->RemoveConnection(conn);
}
