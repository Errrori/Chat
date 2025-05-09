#include "PublicChatController.h"
#include "ConnectionManager.h"      
#include "Utils.h"

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
    if (!json_msg.isMember("is_posted"))
    {
        LOG_ERROR << "can not find flag: is_posted";
        return;
    }
    bool is_posted = json_msg["is_posted"].asBool();
    if (is_posted) return;
	LOG_INFO << "message need to post,sender :"<<info->username;
    json_msg["is_posted"] = true;
    json_msg["uid"] = info->uid;
    json_msg["sender"] = info->username;
    ConnectionManager::GetInstance().BroadcastMsg(info->uid, json_msg.toStyledString());

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
