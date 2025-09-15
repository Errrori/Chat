#include "pch.h"
#include "AIController.h"

#include "AIService.h"
#include "manager/ConnectionManager.h"
#include <curl/curl.h>

#include "manager/ThreadManager.h"

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
	    LOG_ERROR<<"can not parse the message!";
        conn->send("can not parse the message!");
		return;
    }

    if (!msg_data.isMember("content")||!msg_data.isMember("thread_id"))
    {
        conn->send("invalid message format");
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
        conn->send("you are not in the chat: " + std::to_string(thread_id));
        return;
    }

    //构造完整消息
    msg_data["sender_uid"] = info->uid;
    msg_data["sender_name"] = info->username;
    msg_data["sender_avatar"] = info->avatar;
    msg_data["create_time"] = msg_data["update_time"] = trantor::Date::now().toDbString();
	//暂不支持attachment
	msg_data["attachment"] = Json::nullValue;
    const auto& user_message = MessageManager::BuildMessage(msg_data);
    if (!user_message.has_value())
    {
        //构造消息失败,消息格式错误
        LOG_ERROR << "Failed to build message";
        return;
    }
    ThreadManager::PushChatRecords(user_message.value());

	LOG_INFO << "handle new message:" << msg;

    //{
    //    "model": "glm-4.5-flash",
    //        "messages" : [
    //    {
    //        "role": "user",
    //            "content" : "写一段关于夏天的文本"
    //    }
    //        ]
    //}

    //实现上下文记忆功能，这里要构造messages数组
    Json::Value messages;
    auto context = ThreadManager::GetAIChatContext(thread_id);
	messages["content"] = msg_data["content"].asString();
	messages["role"] = "user";
    context.append(messages);

	LOG_INFO << "context: " << context.toStyledString();

    Json::Value json_body;
    json_body["messages"] = context;
    json_body["model"] = "glm-4.5-flash";
	//... next time can add more parameters

	//need choose the url according to the model,but here fix it
    const auto& resp = AIService::SendRequest(context,API_KEY::key1,SERVICE_URL::CHAT_URL);

	if (!resp.has_value())
    {
        LOG_ERROR << "can not generate response!";
		conn->send("can not generate response!");
    	return;
    }


    //发送成功就解析回应，存储到数据库中
	//ThreadManager::PushChatRecords(context, thread_id);

    //构造消息回应，这里省略了构造
	conn->sendJson(resp.value());
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
    auto info_ptr = conn->getContext<Utils::UserInfo>();
    if (info_ptr) {
        LOG_INFO << "username: " << info_ptr->username << " : connection close";
        ConnectionManager::GetInstance().RemoveConnection(info_ptr->uid);
    }
}
