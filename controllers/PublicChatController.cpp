#include "pch.h"
#include "PublicChatController.h"
#include "../manager/ConnectionManager.h"
#include "MsgDispatcher.h"

//Message types:
//Image,Text,ErrorMessage,Relationship

void PublicChatController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& msg,
                                            const drogon::WebSocketMessageType& type)
{
    if (msg.empty())
    {
	    return;
    }
    Json::Value msg_data;
    Json::Reader reader;
    //现在暂时只会检查消息是否是json格式
    if (!reader.parse(msg,msg_data))
    {
        LOG_ERROR << "fail to parse message:"<<msg;
        Json::Value data;
        data["message_type"] = "ErrorMsg";
        data["content"] = "fail to parse the message";
        conn->sendJson(data);
        return;
    }

    Json::Value json_msg;
    json_msg["create_time"] = json_msg["update_time"] = trantor::Date::now().toDbString();
    if(msg_data.isMember("attachment"))
    {
		json_msg["attachment"] = Json::Value(msg_data["attachment"]);
    }
    else{
        json_msg["attachment"] = Json::nullValue;
    }

    json_msg["content"] = msg_data["content"].asString();
    json_msg["thread_id"] = msg_data["thread_id"].asInt();
    json_msg["message_id"] = std::to_string(Utils::Message::GenerateMsgId());

    auto info = conn->getContext<Utils::UserInfo>();
    if (!info)
    {
        LOG_ERROR << "can not get user info";
        return;
    }

    LOG_INFO << "get user info: " << info->ToString();
    

    json_msg["sender_uid"] = info->uid;
    json_msg["sender_name"] = info->username;
    json_msg["sender_avatar"] = info->avatar;

	MsgDispatcher::DeliverMessage(info->uid, msg_data["thread_id"].asInt(), json_msg);
    

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
    else
    {
        LOG_INFO << "add new connection: "+info.ToString();
    }

}

void PublicChatController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    auto info_ptr = conn->getContext<Utils::UserInfo>();
    if (info_ptr) {
		LOG_INFO << "username: " << info_ptr->username << " : connection close";
        ConnectionManager::GetInstance().RemoveConnection(info_ptr->uid);
    }
}
