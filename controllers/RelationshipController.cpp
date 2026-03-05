#include "pch.h"
#include "RelationshipController.h"
#include "Service/RelationshipService.h"
#include "Common/User.h"
#include "Utils.h"
#include "Container.h"

drogon::Task<drogon::HttpResponsePtr> RelationshipController::SendFriendRequest(drogon::HttpRequestPtr req)
{
	try
	{
		auto userInfo = req->getAttributes()->get<UserInfo>("visitor_info");
		std::string requester_uid = userInfo.getUid();

		auto jsonBody = req->getJsonObject();
		if (!jsonBody)
		{
			co_return Utils::CreateErrorResponse(400, 400, "Invalid JSON format");
		}

		if (!jsonBody->isMember("acceptor_uid"))
		{
			co_return Utils::CreateErrorResponse(400, 400, "Missing acceptor_uid");
		}

		std::string acceptor_uid = (*jsonBody)["acceptor_uid"].asString();
		std::string message = "";
		if (jsonBody->isMember("message"))
			message = (*jsonBody)["message"].asString();

		if (requester_uid == acceptor_uid)
		{
			co_return Utils::CreateErrorResponse(400, 400, "Cannot send friend request to yourself");
		}

		auto service = GET_RELATIONSHIP_SERVICE;
		co_await service->SendFriendRequest(requester_uid, acceptor_uid, message);

		co_return Utils::CreateSuccessResp(200, 200, "Friend request sent successfully");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Error in SendFriendRequest: " << e.what();
		co_return Utils::CreateErrorResponse(400,400, "request is already existed");
	}
}

drogon::Task<drogon::HttpResponsePtr> RelationshipController::ProcessFriendRequest(drogon::HttpRequestPtr req)
{
	try
	{
		const auto& userInfo = req->getAttributes()->get<UserInfo>("visitor_info");
		const auto& acceptor_uid = userInfo.getUid(); // The current user is processing the request

		auto jsonBody = req->getJsonObject();
		if (!jsonBody)
		{
			co_return Utils::CreateErrorResponse(400, 400, "Invalid JSON format");
		}

		if (!jsonBody->isMember("requester_uid") || !jsonBody->isMember("action"))
		{
			co_return Utils::CreateErrorResponse(400, 400, "Missing requester_uid or action");
		}

		std::string requester_uid = (*jsonBody)["requester_uid"].asString();
		int action = (*jsonBody)["action"].asInt();

		if (action != 1 && action != 2)
			co_return Utils::CreateErrorResponse(400, 400, "Invalid action (1=Accept, 2=Reject)");

		auto service = GET_RELATIONSHIP_SERVICE;
		try
		{
			int64_t thread_id = co_await service->ProcessFriendRequest(requester_uid, acceptor_uid, action);
			Json::Value data;
			if (thread_id <= 0)
				data = Json::nullValue;
			else
				data["thread_id"] = thread_id;
			co_return Utils::CreateSuccessJsonResp(200, 200, "Friend request processed successfully", data);
		}
		catch (const std::exception& e)
		{
			LOG_ERROR << "exception:"<< e.what();
			co_return Utils::CreateErrorResponse(400, 400, "fail to process the friend request");
		}
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Error in ProcessFriendRequest: " << e.what();
		co_return Utils::CreateErrorResponse(500, 500, "Internal Server Error");
	}
}
