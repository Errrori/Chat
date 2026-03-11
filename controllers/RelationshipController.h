#pragma once
class RelationshipController:public drogon::HttpController<RelationshipController>
{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SendFriendRequest,    "/relation/send-friend-request",      drogon::Post,"TokenVerifyFilter");
        ADD_METHOD_TO(ProcessFriendRequest, "/relation/process-friend-request",   drogon::Post,"TokenVerifyFilter");
        ADD_METHOD_TO(GetUnreadNotifications, "/relation/notifications/unread",   drogon::Get,  "TokenVerifyFilter");
        ADD_METHOD_TO(GetNotifications,     "/relation/notifications",             drogon::Get,"TokenVerifyFilter");
        ADD_METHOD_TO(MarkNotificationsRead,"/relation/notifications/mark-read",  drogon::Post,"TokenVerifyFilter");
        ADD_METHOD_TO(GetPendingFriendRequests,"/relation/friend-requests",       drogon::Get,  "TokenVerifyFilter");
        ADD_METHOD_TO(BlockUser,            "/relation/block",                    drogon::Post,"TokenVerifyFilter");
        ADD_METHOD_TO(UnblockUser,          "/relation/unblock",                  drogon::Post,"TokenVerifyFilter");
    METHOD_LIST_END

	static drogon::Task<drogon::HttpResponsePtr> SendFriendRequest(drogon::HttpRequestPtr req);
	static drogon::Task<drogon::HttpResponsePtr> ProcessFriendRequest(drogon::HttpRequestPtr req);
	static drogon::Task<drogon::HttpResponsePtr> GetUnreadNotifications(drogon::HttpRequestPtr req);
	static drogon::Task<drogon::HttpResponsePtr> GetNotifications(drogon::HttpRequestPtr req);
	static drogon::Task<drogon::HttpResponsePtr> MarkNotificationsRead(drogon::HttpRequestPtr req);
	static drogon::Task<drogon::HttpResponsePtr> GetPendingFriendRequests(drogon::HttpRequestPtr req);
	static drogon::Task<drogon::HttpResponsePtr> BlockUser(drogon::HttpRequestPtr req);
	static drogon::Task<drogon::HttpResponsePtr> UnblockUser(drogon::HttpRequestPtr req);
}; 

