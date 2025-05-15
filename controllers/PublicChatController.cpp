#include "pch.h"
#include "PublicChatController.h"
#include "../manager/ConnectionManager.h"      
#include "../manager/DatabaseManager.h"

//消息类型：
//Image,Text,ErrorMessage,Notice

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
        //查看消息类型,如果缺失message_type就报错
        if (!json_msg.isMember("message_type"))
        {
            LOG_ERROR << "receive error message that misses message_type";
            Json::Value data;
            data["message_type"] = "ErrorMessage";
            data["content_type"] = "Missing field : message_type";
            conn->sendJson(data);
            return;
        }
        //缺少一个对消息类型的检查
        //暂时没有合适的实现，实现代价和收益不匹配

        //message need to be broadcast
        auto msg_id = Utils::GenerateMsgId();
        json_msg["message_id"] = msg_id;
	    json_msg["sender_uid"] = info->uid;
		json_msg["sender_name"] = info->username;
        LOG_INFO << "Put message into records : " << json_msg.toStyledString();
        std::string create_time_str = trantor::Date::now().toDbString();
        json_msg["create_time"] = create_time_str;
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
    Json::Value data;
    data["sender_name"] = info.username;
    data["sender_uid"] = info.uid;
    data["message_type"] = "Notice";
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
