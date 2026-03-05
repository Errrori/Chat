#pragma once
class RelationshipController:public drogon::HttpController<RelationshipController>
{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SendFriendRequest, "/relation/send-friend-request", drogon::Post, "CORSMiddleware","TokenVerifyFilter");
        ADD_METHOD_TO(ProcessFriendRequest, "/relation/process-friend-request", drogon::Post, "CORSMiddleware","TokenVerifyFilter");
    METHOD_LIST_END

	static drogon::Task<drogon::HttpResponsePtr> SendFriendRequest(drogon::HttpRequestPtr req);
	static drogon::Task<drogon::HttpResponsePtr> ProcessFriendRequest(drogon::HttpRequestPtr req);
}; 

