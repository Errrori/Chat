#include "Websocket.h"

using namespace drogon;

void Websocket::handleNewMessage(const WebSocketConnectionPtr& ws, std::string&& msg, const WebSocketMessageType& type)
{
	ws->send("Server receive: " + msg);
}

void Websocket::handleNewConnection(const HttpRequestPtr& req, const WebSocketConnectionPtr& ws)
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
    else if (auto jsonObj = req->getJsonObject(); jsonObj && (*jsonObj).isMember("token")) {
        token = (*jsonObj)["token"].asString();
    }

    // 3.find token in parameter
    else if (!req->getParameter("token").empty()) {
        token = req->getParameter("token");
    }

    if (token != s_token)
    {
        ws->send("fail to verify token");
        ws->shutdown();
        return;
    }
    ws->send("Connection built success,hello client");
}

void Websocket::handleConnectionClosed(const WebSocketConnectionPtr& ws)
{
	LOG_INFO << "connection closed";
	ws->send("Connection closed");
}
