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
//{
//    if (msg.empty()) return;
//    Json::Value msg_data;
//    Json::Reader reader;
//    if (!reader.parse(msg, msg_data))
//    {
//        Json::Value error_msg = Utils::Message::GenerateErrorMsg("can not parse the message: " + msg);
//        conn->sendJson(error_msg);
//        return;
//    }
//    
//
//
//    //创建要发送的信息
//    deliver_msg["content"] = msg_data["content"].asString();
//    deliver_msg["content_type"] = msg_data["content_type"];
//    deliver_msg["target_id_type"] = msg_data["target_id_type"];
//    deliver_msg["target_id"] = msg_data["target_id"];
//
//    //这里应该判断发送目标然后发送，暂时只考虑一对一
//    if (!DeliverMessage(deliver_msg))
//    {
//        conn->sendJson(Utils::Message::GenerateErrorMsg("can not send message"));
//    }
//
//}

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
}


void ChatController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    LOG_INFO << "client connection closed";
    ConnectionManager::GetInstance().RemoveConnection(conn);
}


