#include "EchoController.h"
#include "ConnectionManager.h"
#include "Utils.h"

using namespace drogon;

void EchoController::handleNewMessage(const WebSocketConnectionPtr& conn, std::string&& msg, const WebSocketMessageType& type)
{
    LOG_INFO << "receive msg:" << msg;
    if (msg.empty())
    {
        return;
    }
	conn->send("Server receive: " + msg);
}

void EchoController::handleNewConnection(const HttpRequestPtr& req, const WebSocketConnectionPtr& conn)
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
    if (!Utils::VerifyJWT(token,info))
    {
        conn->send("fail to verify token");
        conn->shutdown();
        return;
    }
    auto uid = info.uid;
    if (!ConnectionManager::GetInstance().AddConnection(info,conn))
    {
        LOG_ERROR << "can not add connection!";

    }
    conn->send("Connection built success.Welcome: "+info.username);
}

void EchoController::handleConnectionClosed(const WebSocketConnectionPtr& conn)
{
	LOG_INFO << "connection closed";
    auto info = conn->getContext<Utils::UserInfo>();
    ConnectionManager::GetInstance().RemoveConnection(info->uid);
	conn->send("Connection closed");
}
