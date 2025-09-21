#include "pch.h"
#include "AIController.h"

#include "AIService.h"
#include "manager/ConnectionManager.h"
#include <curl/curl.h>

#include "manager/ThreadManager.h"
#include "models/AiChats.h"

void AIController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& msg,
                                    const drogon::WebSocketMessageType& type)
{
    if (msg.empty())
    {
        return;
    }

	Json::Value msg_data;
    Json::Reader reader;
    if (!reader.parse(msg, msg_data))
    {
        conn->send("can not parse the message!");
		return;
    }

    if (!msg_data.isMember("content")||!msg_data.isMember("thread_id"))
    {
        conn->send("invalid message format");
        return;
    }

    if (msg_data["content"].asString().empty())
    {
        conn->send("message content can not be empty!");
        return;
    }

    auto thread_id = msg_data["thread_id"].asInt();

    auto info = conn->getContext<Utils::UserInfo>();
    if (!info)
    {
        conn->send("can not get user info from connection!");
        return;
    }

	int thread_type;
    //验证用户是否有权限发言
    if (!ThreadManager::ValidateMember(info->uid, thread_id))
    {
        conn->send("you are not in the chat: " + std::to_string(thread_id));
        return;
    }

   
}

void AIController::handleNewConnection(const drogon::HttpRequestPtr& req, const drogon::WebSocketConnectionPtr& conn)
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
        conn->send("Failed to verify token");
        conn->shutdown();
        return;
    }

    if (!ConnectionManager::GetInstance().AddConnection(info, conn))
    {
        LOG_ERROR << "can not add connection!";
    }
    else
    {
        LOG_INFO << "add new connection,user name: " + info.username;
    }
}

void AIController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    const auto& info_ptr = conn->getContext<Utils::UserInfo>();
    if (info_ptr) {
        LOG_INFO << "username: " << info_ptr->username << " : connection close";
        ConnectionManager::GetInstance().RemoveConnection(info_ptr->uid);
    }
}
