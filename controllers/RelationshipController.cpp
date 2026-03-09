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
		const auto& requester_uid = userInfo.getUid();

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

		auto service = Container::GetInstance().GetRelationshipService();
		co_await service->SendFriendRequest(requester_uid, acceptor_uid, message);

		co_return Utils::CreateSuccessResp(200, 200, "Friend request sent successfully");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Error in SendFriendRequest: " << e.what();
		co_return Utils::CreateErrorResponse(400, 400, e.what());
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

		auto service = Container::GetInstance().GetRelationshipService();
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

drogon::Task<drogon::HttpResponsePtr> RelationshipController::GetUnreadNotifications(drogon::HttpRequestPtr req)
{
	try
	{
		const auto& userInfo = req->getAttributes()->get<UserInfo>("visitor_info");
		auto data = co_await Container::GetInstance().GetRelationshipService()->GetUnreadNotifications(userInfo.getUid());
		co_return Utils::CreateSuccessJsonResp(200, 200, "success", data);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Error in GetUnreadNotifications: " << e.what();
		co_return Utils::CreateErrorResponse(500, 500, "Internal Server Error");
	}
}

drogon::Task<drogon::HttpResponsePtr> RelationshipController::GetNotifications(drogon::HttpRequestPtr req)
{
	try
	{
		const auto& userInfo = req->getAttributes()->get<UserInfo>("visitor_info");

		int offset = req->getOptionalParameter<int>("offset").value_or(0);
		int limit  = req->getOptionalParameter<int>("limit").value_or(0);
		//limit the value between 1 and 100
		limit  = std::clamp(limit, 1, 100);

		auto data = co_await Container::GetInstance().GetRelationshipService()->GetNotifications(userInfo.getUid(), offset, limit);
		co_return Utils::CreateSuccessJsonResp(200, 200, "success", data);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Error in GetNotifications: " << e.what();
		co_return Utils::CreateErrorResponse(500, 500, "Internal Server Error");
	}
}

drogon::Task<drogon::HttpResponsePtr> RelationshipController::MarkNotificationsRead(drogon::HttpRequestPtr req)
{
	try
	{
		const auto& userInfo = req->getAttributes()->get<UserInfo>("visitor_info");
		const auto& uid = userInfo.getUid();

		std::vector<int64_t> ids;
		auto jsonBody = req->getJsonObject();
		if (jsonBody && jsonBody->isMember("ids") && (*jsonBody)["ids"].isArray())
		{
			for (const auto& v : (*jsonBody)["ids"])
			{
				if (v.isInt64())
					ids.push_back(v.asInt64());
			}
		}

		size_t updated = co_await Container::GetInstance().GetRelationshipService()->MarkNotificationsRead(uid, ids);

		Json::Value data;
		data["updated"] = static_cast<int>(updated);
		co_return Utils::CreateSuccessJsonResp(200, 200, "success", data);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Error in MarkNotificationsRead: " << e.what();
		co_return Utils::CreateErrorResponse(500, 500, "Internal Server Error");
	}
}

drogon::Task<drogon::HttpResponsePtr> RelationshipController::GetPendingFriendRequests(drogon::HttpRequestPtr req)
{
	try
	{
		const auto& userInfo = req->getAttributes()->get<UserInfo>("visitor_info");
		auto data = co_await Container::GetInstance().GetRelationshipService()->GetPendingFriendRequests(userInfo.getUid());
		co_return Utils::CreateSuccessJsonResp(200, 200, "success", data);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Error in GetPendingFriendRequests: " << e.what();
		co_return Utils::CreateErrorResponse(500, 500, "Internal Server Error");
	}
}

drogon::Task<drogon::HttpResponsePtr> RelationshipController::BlockUser(drogon::HttpRequestPtr req)
{
	try
	{
		const auto& userInfo = req->getAttributes()->get<UserInfo>("visitor_info");
		const auto& operator_uid = userInfo.getUid();

		auto jsonBody = req->getJsonObject();
		if (!jsonBody || !jsonBody->isMember("target_uid"))
			co_return Utils::CreateErrorResponse(400, 400, "Missing target_uid");

		std::string target_uid = (*jsonBody)["target_uid"].asString();
		if (operator_uid == target_uid)
			co_return Utils::CreateErrorResponse(400, 400, "Cannot block yourself");

		try
		{
			co_await Container::GetInstance().GetRelationshipService()->BlockUser(operator_uid, target_uid);
		}
		catch (const std::invalid_argument& e)
		{
			co_return Utils::CreateErrorResponse(400, 400, e.what());
		}
		co_return Utils::CreateSuccessResp(200, 200, "User blocked successfully");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Error in BlockUser: " << e.what();
		co_return Utils::CreateErrorResponse(500, 500, "Internal Server Error");
	}
}

drogon::Task<drogon::HttpResponsePtr> RelationshipController::UnblockUser(drogon::HttpRequestPtr req)
{
	try
	{
		const auto& userInfo = req->getAttributes()->get<UserInfo>("visitor_info");
		const auto& operator_uid = userInfo.getUid();

		auto jsonBody = req->getJsonObject();
		if (!jsonBody || !jsonBody->isMember("target_uid"))
			co_return Utils::CreateErrorResponse(400, 400, "Missing target_uid");

		std::string target_uid = (*jsonBody)["target_uid"].asString();
		if (operator_uid == target_uid)
			co_return Utils::CreateErrorResponse(400, 400, "Cannot unblock yourself");

		try
		{
			co_await Container::GetInstance().GetRelationshipService()->UnblockUser(operator_uid, target_uid);
		}
		catch (const std::invalid_argument& e)
		{
			co_return Utils::CreateErrorResponse(400, 400, e.what());
		}
		co_return Utils::CreateSuccessResp(200, 200, "User unblocked successfully");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Error in UnblockUser: " << e.what();
		co_return Utils::CreateErrorResponse(500, 500, "Internal Server Error");
	}
}
