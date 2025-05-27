#include "pch.h"
#include "ChatController.h"

#include "manager/ConnectionManager.h"
using ChatType = Utils::Message::Chat::ChatType;

void ChatController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& msg,
    const drogon::WebSocketMessageType& type) 
{
    if (msg.empty()) return;
    Json::Value json_msg;
    Json::Reader reader;
    if (!reader.parse(msg, json_msg)) {
        conn->sendJson(Utils::Message::GenerateErrorMsg("can not parse the message: " + msg));
        return;
    }
    SendManager::GetInstance().PushMessage(json_msg, conn);
}

void ChatController::handleNewConnection(const drogon::HttpRequestPtr& req,
	const drogon::WebSocketConnectionPtr& conn)
{
    std::string token = Utils::Authentication::GetToken(req);

    Utils::UserInfo info;
    if (!Utils::Authentication::VerifyJWT(token, info))
    {
        LOG_ERROR << "error to verify token";
        Json::Value error_msg = Utils::Message::GenerateErrorMsg("can not verify token");
        conn->sendJson(error_msg);
        conn->shutdown();
        return;
    }
    if (!ConnectionManager::GetInstance().AddConnection(info, conn))
    {
        LOG_ERROR << "error to add connection";
        Json::Value error_msg = Utils::Message::GenerateErrorMsg("can not add connection!");
        conn->sendJson(error_msg);
        conn->shutdown();
    }
    LOG_INFO << "new client connected: " << info.username;

    Json::Value data(Json::arrayValue);
    if (!DatabaseManager::GetUserNotification(info.uid, data))
    {
        LOG_ERROR << "fail to get notifications";
    }
    else
    {
        Json::Value notification;
        notification["total"] = data.size();
        notification["content"] = data;
        notification["chat_type"] = "System";
        conn->sendJson(notification);
    }
}


void ChatController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    LOG_INFO << "client connection closed";
    ConnectionManager::GetInstance().RemoveConnection(conn);
}


