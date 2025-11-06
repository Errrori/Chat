#pragma once
#include <drogon/drogon.h>
class ThreadController:public drogon::HttpController<ThreadController>
{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ThreadController::CreatePrivateThread, "/thread/create/private-chat", drogon::Post, "CORSMiddleware", "TokenVerifyFilter");
        ADD_METHOD_TO(ThreadController::CreateGroupThread, "/thread/create/group-chat", drogon::Post, "CORSMiddleware", "TokenVerifyFilter");
        ADD_METHOD_TO(ThreadController::CreateAIThread, "/thread/create/ai-chat", drogon::Post, "CORSMiddleware", "TokenVerifyFilter");
        ADD_METHOD_TO(ThreadController::QueryThreadInfo, "/thread/info/query", drogon::Get, "CORSMiddleware", "TokenVerifyFilter");
		ADD_METHOD_TO(ThreadController::AddThreadMember, "/thread/group/add-member", drogon::Post, "CORSMiddleware", "TokenVerifyFilter");
		ADD_METHOD_TO(ThreadController::AddThreadMember, "/thread/group/join", drogon::Post, "CORSMiddleware", "TokenVerifyFilter");
		ADD_METHOD_TO(ThreadController::GetAIContext, "/thread/ai-context", drogon::Get, "CORSMiddleware");
        
	METHOD_LIST_END

private:
    void CreatePrivateThread(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void CreateGroupThread(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void CreateAIThread(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void QueryThreadInfo(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void AddThreadMember(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void JoinThread(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);


    void GetAIContext(const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

};

