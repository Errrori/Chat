#pragma once
#include <drogon/WebSocketController.h>

class Websocket :public drogon::WebSocketController<Websocket>
{
public:
    void handleNewMessage(const drogon::WebSocketConnectionPtr& ws,
        std::string&& msg,
        const drogon::WebSocketMessageType& type) override;
    void handleNewConnection(const drogon::HttpRequestPtr& req,
        const drogon::WebSocketConnectionPtr& ws) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& ws) override;

    WS_PATH_LIST_BEGIN
        WS_PATH_ADD("/ws/echo");
	WS_PATH_LIST_END

};

