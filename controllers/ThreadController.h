#pragma once
#include <drogon/drogon.h>
class ThreadController:public drogon::HttpController<ThreadController>
{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ThreadController::AddToThread, "/thread/group/invite", drogon::Post, "CorsMiddleware", "TokenVerifyFilter");
        ADD_METHOD_TO(ThreadController::JoinThread, "/thread/group/join", drogon::Post, "CorsMiddleware", "TokenVerifyFilter");
		ADD_METHOD_TO(ThreadController::GetThreadInfo, "/thread/group/info", drogon::Get, "CorsMiddleware", "TokenVerifyFilter");
		ADD_METHOD_TO(ThreadController::CreatePrivateChats, "/thread/create/private", drogon::Post, "CorsMiddleware", "TokenVerifyFilter");
		ADD_METHOD_TO(ThreadController::CreateGroupChats, "/thread/create/group", drogon::Post, "CorsMiddleware", "TokenVerifyFilter");
		ADD_METHOD_TO(ThreadController::CreateAIChats, "/thread/create/ai", drogon::Post, "CorsMiddleware", "TokenVerifyFilter");
		ADD_METHOD_TO(ThreadController::GetUserThreadIds, "/thread/user/get_id", drogon::Get, "CorsMiddleware", "TokenVerifyFilter");
		ADD_METHOD_TO(ThreadController::GetOverviewChat, "/thread/record/overview", drogon::Get, "CorsMiddleware", "TokenVerifyFilter");
        ADD_METHOD_TO(ThreadController::GetChatRecords, "/thread/record/get", drogon::Get, "CorsMiddleware", "TokenVerifyFilter");
        ADD_METHOD_TO(ThreadController::FindThreadInfo, "/thread/info", drogon::Get, "CorsMiddleware", "TokenVerifyFilter");

        ADD_METHOD_TO(ThreadController::CreatePrivateThread, "/thread/create/private-chat", drogon::Post, "CorsMiddleware", "TokenVerifyFilter");
        ADD_METHOD_TO(ThreadController::CreateGroupThread, "/thread/create/group-chat", drogon::Post, "CorsMiddleware", "TokenVerifyFilter");
        ADD_METHOD_TO(ThreadController::CreateAIThread, "/thread/create/ai-chat", drogon::Post, "CorsMiddleware", "TokenVerifyFilter");
        ADD_METHOD_TO(ThreadController::QueryThreadInfo, "/thread/info/query", drogon::Get, "CorsMiddleware", "TokenVerifyFilter");
    METHOD_LIST_END

private:
    void JoinThread(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void AddToThread(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void GetThreadInfo(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void CreateGroupChats(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void CreateAIChats(const drogon::HttpRequestPtr& req,
		std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void CreatePrivateChats(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void GetUserThreadIds(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void GetChatRecords(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void GetOverviewChat(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void FindThreadInfo(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void CreatePrivateThread(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void CreateGroupThread(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void CreateAIThread(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void QueryThreadInfo(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    

};

