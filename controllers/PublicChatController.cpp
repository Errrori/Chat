#include "pch.h"
#include "PublicChatController.h"
#include "../manager/ConnectionManager.h"      
#include "../manager/DatabaseManager.h"

//Message types:
//Image,Text,ErrorMessage,Relationship

void PublicChatController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& msg,
                                            const drogon::WebSocketMessageType& type)
{
    if (msg.empty()) return;
    
    Json::Value json_msg;
    Json::Reader reader;
    if (!reader.parse(msg,json_msg)||!json_msg.isMember("message_type"))
    {
        LOG_ERROR << "fail to parse message:"<<msg;
        Json::Value data;
        data["message_type"] = "ErrorMsg";
        data["content_type"] = "fail to parse the message";
        conn->sendJson(data);
        return;
    }

    const auto& msg_type = json_msg["chat_type"].asString();
	if (!Utils::Message::Chat::IsValid(msg_type))
	{
        LOG_ERROR << "the message type is not supported";
        Json::Value data;
        data["message_type"] = "ErrorMsg";
        data["content_type"] = "the message type is not supported";
        conn->sendJson(data);
        return;
	}

    if (!json_msg.isMember("forwarded"))
    {
        auto info = conn->getContext<Utils::UserInfo>();
        //message need to be broadcast
        auto msg_id = Utils::Message::GenerateMsgId();
        json_msg["message_id"] = msg_id;
	    json_msg["sender_uid"] = info->uid;
		json_msg["sender_name"] = info->username;
        LOG_INFO << "Put message into records : " << json_msg.toStyledString();
        std::string create_time_str = trantor::Date::now().toDbString();
        json_msg["create_time"] = create_time_str;
        DatabaseManager::PushChatRecords(json_msg);
        //Add to records before setting forwarded
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
    
    Json::Value data;
    data["sender_name"] = info.username;
    data["sender_uid"] = info.uid;
    data["message_type"] = "Relationship";
    data["forward"] = true;
    ConnectionManager::GetInstance().BroadcastMsg(info.uid,data);
}

void PublicChatController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    auto info_ptr = conn->getContext<Utils::UserInfo>();
    if (info_ptr) {
		LOG_INFO << "username: " << info_ptr->username << " : connection close";
    }
    ConnectionManager::GetInstance().RemoveConnection(info_ptr->uid);
}
