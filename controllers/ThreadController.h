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
		ADD_METHOD_TO(ThreadController::JoinThread, "/thread/group/join", drogon::Post, "CORSMiddleware", "TokenVerifyFilter");
		ADD_METHOD_TO(ThreadController::GetAIContext, "/thread/record/ai", drogon::Get, "CORSMiddleware", "TokenVerifyFilter");
		ADD_METHOD_TO(ThreadController::GetChatRecords, "/thread/record/user", drogon::Get, "CORSMiddleware", "TokenVerifyFilter");
        ADD_METHOD_TO(ThreadController::GetChatOverviews,"/thread/record/overview",drogon::Get, "CORSMiddleware", "TokenVerifyFilter");

	METHOD_LIST_END

private:
    drogon::Task<drogon::HttpResponsePtr> CreatePrivateThread(drogon::HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr> CreateGroupThread(drogon::HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr> CreateAIThread(drogon::HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr> QueryThreadInfo(drogon::HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr> AddThreadMember(drogon::HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr> JoinThread(drogon::HttpRequestPtr req);


    drogon::Task<drogon::HttpResponsePtr> GetAIContext(drogon::HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr> GetChatRecords(drogon::HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr> GetChatOverviews(drogon::HttpRequestPtr req);

};

