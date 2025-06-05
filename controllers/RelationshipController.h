#pragma once
#include <drogon/drogon.h>

class RelationshipController:public drogon::HttpController<RelationshipController>
{
public:
	METHOD_LIST_BEGIN
		ADD_METHOD_TO(RelationshipController::HandleSendFriendRequest, "relationship/friend_request/send_request", drogon::Post, "CorsMiddleware","TokenVerifyFilter");
		ADD_METHOD_TO(RelationshipController::HandleRequestAccept,"/relationship/friend_request/accept",drogon::Post,"CorsMiddleware","TokenVerifyFilter");
		ADD_METHOD_TO(RelationshipController::HandleRequestReject,"/relationship/friend_request/reject",drogon::Post,"CorsMiddleware","TokenVerifyFilter");
		ADD_METHOD_TO(RelationshipController::HandleUnfriend,"/relationship/unfriend",drogon::Post, "CorsMiddleware","TokenVerifyFilter");
		ADD_METHOD_TO(RelationshipController::HandleBlockUser,"/relationship/block_user",drogon::Post, "CorsMiddleware","TokenVerifyFilter");
		ADD_METHOD_TO(RelationshipController::HandleUnblockUser,"/relationship/unblock_user",drogon::Post, "CorsMiddleware","TokenVerifyFilter");
		ADD_METHOD_TO(RelationshipController::HandleGetFriendships, "/relationship/get_relationships", drogon::Get, "CorsMiddleware");
	METHOD_LIST_END
private:
	void HandleSendFriendRequest(const drogon::HttpRequestPtr& req,
		std::function<void(const drogon::HttpResponsePtr&)>&& callback);
	void HandleRequestAccept(const drogon::HttpRequestPtr& req,
		std::function<void(const drogon::HttpResponsePtr&)>&& callback);
	void HandleRequestReject(const drogon::HttpRequestPtr& req,
		std::function<void(const drogon::HttpResponsePtr&)>&& callback);
	void HandleBlockUser(const drogon::HttpRequestPtr& req,
		std::function<void(const drogon::HttpResponsePtr&)>&& callback);
	void HandleUnblockUser(const drogon::HttpRequestPtr& req,
		std::function<void(const drogon::HttpResponsePtr&)>&& callback);
	void HandleUnfriend(const drogon::HttpRequestPtr& req,
		std::function<void(const drogon::HttpResponsePtr&)>&& callback);
	void HandleGetFriendships(const drogon::HttpRequestPtr& req,
		std::function<void(const drogon::HttpResponsePtr&)>&& callback);
	bool ValidateBody(const drogon::HttpRequestPtr& req);
	bool GetDTOFromRequest(const drogon::HttpRequestPtr& req, RelationshipDTO& dto, std::string& error_msg);
	//void HandleFollowFriend();
};

