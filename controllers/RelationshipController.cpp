#include "pch.h"
#include "RelationshipController.h"

#include "manager/SendManager.h"

void RelationshipController::HandleSendFriendRequest(const drogon::HttpRequestPtr& req,
                                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	const auto& data = *(req->getJsonObject());
	if (!ValidateBody(req))
	{
		callback(Utils::CreateErrorResponse(400, 400, "lack of essential field"));
		return;
	}
	//if user is blocked
	//...

	//if user is not blocked by receiver,send the request
	//...
	Json::Value request_msg;
	request_msg["actor_uid"] = data["sender_uid"].asString();
	request_msg["reactor_uid"] = data["receiver_uid"].asString();
	request_msg["content"] = data["content"].asString();
	request_msg["create_time"] = trantor::Date::now().toDbString();
	request_msg["action_type"] = data["action_type"].asString();
	//要分在线和离线情况，在线要直接通知，不在线则写入通知表,但是一定要写入关系表，pending关系
	DatabaseManager::PushNotification(request_msg);
	Json::Value json_resp;
	json_resp["code"] = 200;
	json_resp["message"] = "success send request";
	callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));
}

void RelationshipController::HandleAcceptRequest(const drogon::HttpRequestPtr& req,
                                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
}

void RelationshipController::HandleRejectRequest(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
}

void RelationshipController::HandleBlockUser(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
}

void RelationshipController::HandleUnblockUser(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
}

void RelationshipController::HandleUnfriend(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
}

bool RelationshipController::ValidateBody(const drogon::HttpRequestPtr& req)
{
	const auto& body = req->getJsonObject();
	if (!body->isMember("sender_uid")||!body->isMember("receiver_uid"))
	{
		LOG_ERROR << "lack of uid";
		return false;
	}
	auto sender_uid = (*body)["sender_uid"].asString();
	auto visitor_info = req->getAttributes()->get<Utils::UserInfo>("visitor_info");
	if (visitor_info.uid != sender_uid)
	{
		LOG_ERROR << "sender info is not the same as body info";
		return false;
	}
	return true;
}
