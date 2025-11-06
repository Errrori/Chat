#pragma once
#include <drogon/WebSocketController.h>

class UserService;

class ChatController :public drogon::WebSocketController<ChatController>
{
public:
    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
        std::string&& msg,
        const drogon::WebSocketMessageType& type) override;
    void handleNewConnection(const drogon::HttpRequestPtr& req,
        const drogon::WebSocketConnectionPtr& conn) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;

    WS_PATH_LIST_BEGIN
        WS_PATH_ADD("/ws/chat");
    WS_PATH_LIST_END

    struct ChatMessages
    {
        int thread_id;
        std::string message_id;
        std::string sender_name;
        std::string sender_uid;
        std::string sender_avatar;
        std::string content;
        Json::Value attachment{ Json::nullValue };
        std::string create_time;
        std::string update_time;

        static std::optional<ChatMessages> FromJson(const Json::Value& json);
    };


private:
    std::optional<Json::Value> ParseMessage(const std::string& message, std::string& error) const;
};



