#include "pch.h"
#include "ChatController.h"
#include "Service/ConnectionService.h"
#include "models/AiChats.h"

#include "Container.h"
#include "Common/ChatMessage.h"
#include "Service/MessageService.h"

//Message types:
//Image,Text,ErrorMessage,Relationship


void ChatController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& msg,
                                            const drogon::WebSocketMessageType& type)
{
    try
    {
        if (msg.empty())
        {
            return;
        }

        Json::Value msg_data;
        Json::Reader reader;
        if (!reader.parse(msg, msg_data))
        {
            LOG_ERROR << "can not parse message";
            conn->sendJson(Utils::GenErrorResponse("fail to parse message",ChatCode::InValidJson));
            return;
        }

        if ((!msg_data.isMember("content") && !msg_data.isMember("attachment"))
            || !msg_data.isMember("thread_id"))
        {
            LOG_ERROR << "lack of essential field";
			conn->sendJson(Utils::GenErrorResponse("lack of essential field", ChatCode::MissingField));
            return;
        }

        const auto& conn_info = GET_CONN_SERVICE->GetConnInfo(conn);

        if (msg_data.isMember("request_data"))
        {
            GET_MESSAGE_SERVICE->ProcessAIRequest(std::move(msg_data), conn);
            return;
        }

        auto message = ChatMessage::FromJson(msg_data);

        message.setSenderUid(conn_info->uid);
        message.setSenderName(conn_info->username);
        message.setSenderAvatar(conn_info->avatar);
        GET_MESSAGE_SERVICE->ProcessUserMsg(std::move(message), [conn](const Json::Value& error)
            {
                if (conn && conn->connected())
                    conn->sendJson(error);
                LOG_ERROR << "process message error: " << error.toStyledString();
            });
    }catch (const std::exception& e)
    {
        conn->sendJson(Utils::GenErrorResponse(std::string("system exception: ")+e.what(),ChatCode::SystemException));
    }

}

void ChatController::handleNewConnection(const drogon::HttpRequestPtr& req,
	const drogon::WebSocketConnectionPtr& conn)
{
    std::string token;
    //1.find token in authorization
    std::string authHeader = req->getHeader("Authorization");
    if (!authHeader.empty()) {
        if (authHeader.substr(0, 4) == "JWT ") {
            token = authHeader.substr(4);
        }
        else {
            token = authHeader;
        }
    }
    // 2.find token in Json
    else if (auto jsonObj = req->getJsonObject(); jsonObj && jsonObj->isMember("token")) {
        token = (*jsonObj)["token"].asString();
    }

    // 3.find token in parameter
    else if (!req->getParameter("token").empty()) {
        token = req->getParameter("token");
    }

    Utils::UsersInfo info;
    if (!Utils::Authentication::VerifyJWT(token, info))
    {
        conn->send("Failed to verify token");
        conn->shutdown();
        return;
    }

    ConnectionService::ConnInfo conn_info{ info.uid,info.account,info.username,info.avatar };

    //after verify,write user info
    const auto& conn_service = GET_CONN_SERVICE;

    if (!conn_service->AddConnection(conn,std::move(conn_info)))
    {
        conn->sendJson(Utils::GenErrorResponse("can not add connection",ChatCode::FailAddConn));
        conn->shutdown();
        LOG_ERROR << "can not add connection!";
    }
    else
    {
        LOG_INFO << "add new connection: "+info.ToString();
    }
}

void ChatController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    const auto& info_ptr = conn->getContext<Utils::UsersInfo>();
    if (info_ptr) {
		LOG_INFO << "username: " << info_ptr->username << "  connection close";
    }
}