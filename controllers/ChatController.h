#pragma once
#include <drogon/WebSocketController.h>
#include "manager/SendManager.h"

class ChatController :public drogon::WebSocketController<ChatController>
{
public:
    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
        std::string&& msg,
        const drogon::WebSocketMessageType& type) override;
    void handleNewConnection(const drogon::HttpRequestPtr& req,
        const drogon::WebSocketConnectionPtr& conn) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;

    bool WriteSenderInfo(const drogon::WebSocketConnectionPtr& conn, Json::Value& json_msg);
    WS_PATH_LIST_BEGIN
        WS_PATH_ADD("/ws/chat");
    WS_PATH_LIST_END

};