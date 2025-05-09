#pragma once
#include <drogon/WebSocketController.h>

class PublicChatController :public drogon::WebSocketController<PublicChatController>
{
public:
    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
        std::string&& msg,
        const drogon::WebSocketMessageType& type) override;
    void handleNewConnection(const drogon::HttpRequestPtr& req,
        const drogon::WebSocketConnectionPtr& conn) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;

    WS_PATH_LIST_BEGIN
        WS_PATH_ADD("/ws/chatroom");
    WS_PATH_LIST_END

};

