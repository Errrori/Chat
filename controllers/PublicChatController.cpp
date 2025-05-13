#include "pch.h"
#include "PublicChatController.h"
#include "../manager/ConnectionManager.h"      
#include "../manager/DatabaseManager.h"

//message format:
// {
//      is_posted    //false : need to post , true : do not post
//      message
//      uid
// }

void PublicChatController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& msg,
                                            const drogon::WebSocketMessageType& type)
{
    if (msg.empty()) return;
    auto info = conn->getContext<Utils::UserInfo>();
    LOG_INFO << info->username<<" Receive message : " << msg;

    Json::Value json_msg;
    Json::Reader reader;
    if (!reader.parse(msg,json_msg))
    {
        LOG_ERROR << "fail to parse msg";
    	return;
    }
    if (!json_msg.isMember("forwarded"))
    {
        //message need to be broadcast
        auto msg_id = Utils::GenerateMsgId();
        json_msg["message_id"] = msg_id;
	    json_msg["sender_uid"] = info->uid;
		json_msg["sender_name"] = info->username;
        LOG_INFO << "Put message into records : " << json_msg.toStyledString();
        DatabaseManager::PushChatRecords(json_msg);
        //这里在写入forwarded之前先放入聊天记录
        json_msg["forwarded"] = true;
		ConnectionManager::GetInstance().BroadcastMsg(info->uid, json_msg);
        /*Json::Value reply;
        reply["id"] = json_msg["id"].asString();
        reply["message_id"] = msg_id;
        reply["ack"] = true;*/
        conn->sendJson(json_msg);
    }

    //if program reach here,it indicates that this message do not need to forward,
    //server can check if "forwarded" is true here, but we ignore this
}

void PublicChatController::handleNewConnection(const drogon::HttpRequestPtr& req,
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
    if (!Utils::VerifyJWT(token, info))
    {
        conn->send("fail to verify token");
        conn->shutdown();
        return;
    }

    if (!ConnectionManager::GetInstance().AddConnection(info, conn))
    {
        LOG_ERROR << "can not add connection!";

    }
    conn->send("Connection built success.Welcome: " + info.username);
}

void PublicChatController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    auto info_ptr = conn->getContext<Utils::UserInfo>();
    if (info_ptr) {
		LOG_INFO << "username: " << info_ptr->username << " : connection close";
    }
    ConnectionManager::GetInstance().RemoveConnection(info_ptr->uid);
}
