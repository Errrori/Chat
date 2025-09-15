#include "pch.h"
#include "ChatController.h"
#include "../manager/ConnectionManager.h"
#include "MsgDispatcher.h"
#include "manager/ThreadManager.h"

//Message types:
//Image,Text,ErrorMessage,Relationship

void ChatController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& msg,
                                            const drogon::WebSocketMessageType& type)
{
    if (msg.empty())
    {
	    return;
    }

	LOG_INFO << "new message:" << msg;

    try
    {
        Json::Value msg_data;
        Json::Reader reader;
        //现在暂时只会检查消息是否是json格式
        if (!reader.parse(msg, msg_data))
        {
            LOG_ERROR << "fail to parse message:" << msg;
            Json::Value data;
            data["message_type"] = "ErrorMsg";
            data["content"] = "fail to parse the message";
            conn->sendJson(data);
            return;
        }

        if (!msg_data.isMember("thread_id"))
        {
            conn->send("lack of essential field");
            return;
        }

        auto thread_id = msg_data["thread_id"].asInt();

        auto info = conn->getContext<Utils::UserInfo>();
        if (!info)
        {
            conn->send("can not get user info from connection!");
            return;
        }

        //检查用户是否有权限发言
        if (!ThreadManager::ValidateMember(info->uid, thread_id))
        {
            LOG_ERROR << "can not send";
            conn->send("you are not in the chat: " + std::to_string(thread_id));
            return;
        }

        LOG_INFO << "code get here 1 ";

        //构造完整消息
        msg_data["sender_uid"] = info->uid;
        msg_data["sender_name"] = info->username;
        msg_data["sender_avatar"] = info->avatar;
        msg_data["create_time"] = msg_data["update_time"] = trantor::Date::now().toDbString();

		LOG_INFO << "info->uid: " << info->uid << ", info->username: " << info->username << ", info->avatar: " << info->avatar;

        if (!msg_data.isMember("attachment"))
            msg_data["attachment"] = Json::nullValue;

		LOG_INFO << "message to build: " << msg_data.toStyledString();

        const auto& message = MessageManager::BuildMessage(msg_data);
        if (!message.has_value())
        {
            //构造消息失败,消息格式错误
            LOG_ERROR << "Failed to build message";
            return;
        }

        //推送消息
        if (!MsgDispatcher::DeliverMessage(info->uid, thread_id, msg_data))
        {
            //消息发送失败
            std::cout << "Failed to deliver message";
            LOG_INFO << "code get here!";
            return;
        }

        ThreadManager::PushChatRecords(message.value());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "can not build message from json: " << e.what();
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

void ChatController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    auto info_ptr = conn->getContext<Utils::UserInfo>();
    if (info_ptr) {
		LOG_INFO << "username: " << info_ptr->username << " : connection close";
        ConnectionManager::GetInstance().RemoveConnection(info_ptr->uid);
    }
}
