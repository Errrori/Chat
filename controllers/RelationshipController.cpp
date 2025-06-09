#include "pch.h"
#include "RelationshipController.h"
#include "DTOs/RelationshipDTO.h"
#include "RelationshipService.h"
#include "DTOs/NotificationDTO.h"
#include "manager/NotificationManager.h"

using RelationshipActionType = Utils::UserAction::RelationAction::RelationshipActionType;

void RelationshipController::HandleSendFriendRequest(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	RelationshipDTO dto;
	std::string error_msg;
	if (!GetDTOFromRequest(req, dto,error_msg))
	{
		callback(Utils::CreateErrorResponse(400, 400, error_msg));
		return;
	}

	if (dto.GetActionType()!= RelationshipActionType::FriendRequest)
	{
		callback(Utils::CreateErrorResponse(400, 400, "error route request"));
		return;
	}

	// 检查合法性
	if (RelationshipService::IsPending(dto.GetActorUid(), dto.GetReactorUid())
	    || RelationshipService::IsPending(dto.GetReactorUid(), dto.GetActorUid())
	    || RelationshipService::IsFriend(dto.GetActorUid(), dto.GetReactorUid())
	    || RelationshipService::IsBlocking(dto.GetActorUid(), dto.GetReactorUid()))
	{
	    callback(Utils::CreateErrorResponse(400, 400, "can not send friend request"));
	    return;
	}

	std::string msg;
	if (!DatabaseManager::WriteRelationship(dto,msg))
	{
		LOG_ERROR << msg;
		callback(Utils::CreateErrorResponse(400, 400, "can not write pending relationship to table: "+msg));
		return;
	}

	const auto& notification_dto = NotificationDTO::BuildFromDTO(dto, NotificationSource::Relationship);
	if (!notification_dto.has_value())
	{
		callback(Utils::CreateErrorResponse(400, 400, "can not build notification from relationship DTO "));
		return;
	}
	NotificationManager::GetInstance().PushNotification(notification_dto.value());

	Json::Value json_resp;
	json_resp["code"] = 200;
	json_resp["message"] = "success send request";
	callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));
}

void RelationshipController::HandleRequestAccept(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	RelationshipDTO dto;
	std::string error_msg;
	if (!GetDTOFromRequest(req, dto, error_msg))
	{
		callback(Utils::CreateErrorResponse(400, 400, error_msg));
		return;
	}

	if (dto.GetActionType() != RelationshipActionType::RequestAccept)
	{
		callback(Utils::CreateErrorResponse(400, 400, "error route request"));
		return;
	}

	if (!RelationshipService::IsPending(dto.GetReactorUid(), dto.GetActorUid()))
	{
		callback(Utils::CreateErrorResponse(400, 400, "can not accept friend request"));
		return;
	}

	std::string msg;
	if (!DatabaseManager::WriteRelationship(dto, msg))
	{
		LOG_ERROR << msg;
		callback(Utils::CreateErrorResponse(400, 400, "can not write into relationship table"));
		return;
	}

	const auto& notification_dto = NotificationDTO::BuildFromDTO(dto, NotificationSource::Relationship);
	if (!notification_dto.has_value())
	{
		callback(Utils::CreateErrorResponse(400, 400, "can not build notification from relationship DTO "));
		return;
	}
	NotificationManager::GetInstance().PushNotification(notification_dto.value());


	Json::Value json_resp;
	json_resp["code"] = 200;
	json_resp["message"] = "success send request";
	callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));
}

void RelationshipController::HandleRequestReject(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	RelationshipDTO dto;
	std::string error_msg;
	if (!GetDTOFromRequest(req, dto, error_msg))
	{
		callback(Utils::CreateErrorResponse(400, 400, error_msg));
		return;
	}

	if (dto.GetActionType() != RelationshipActionType::RequestReject)
	{
		callback(Utils::CreateErrorResponse(400, 400, "error route request"));
		return;
	}

	//����Ϸ���
	if (!RelationshipService::IsPending(dto.GetReactorUid(), dto.GetActorUid()))
	{
		callback(Utils::CreateErrorResponse(400, 400, "can not accept friend request"));
		return;
	}

	//�Ϸ���д���ϵ��
	std::string msg;
	if (!DatabaseManager::WriteRelationship(dto, msg))
	{
		LOG_ERROR << msg;
		callback(Utils::CreateErrorResponse(400, 400, "can not write into relationship table"));
		return;
	}
	//Ӧ�ý���֪ͨ


	Json::Value json_resp;
	json_resp["code"] = 200;
	json_resp["message"] = "success send request";
	callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));
}

void RelationshipController::HandleBlockUser(const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	RelationshipDTO dto;
	std::string error_msg;
	if (!GetDTOFromRequest(req, dto, error_msg))
	{
		callback(Utils::CreateErrorResponse(400, 400, error_msg));
		return;
	}

	if (dto.GetActionType() != RelationshipActionType::BlockUser)
	{
		callback(Utils::CreateErrorResponse(400, 400, "error route request"));
		return;
	}

	//����Ϸ���
	if (RelationshipService::IsBlocking(dto.GetActorUid(), dto.GetReactorUid()))
	{
		callback(Utils::CreateErrorResponse(400, 400, "can not block user"));
		return;
	}

	//�Ϸ���д���ϵ��
	std::string msg;
	if (!DatabaseManager::WriteRelationship(dto, msg))
	{
		LOG_ERROR << msg;
		callback(Utils::CreateErrorResponse(400, 400, "can not write into relationship table"));
		return;
	}
	//Ӧ�ý���֪ͨ


	Json::Value json_resp;
	json_resp["code"] = 200;
	json_resp["message"] = "success send request";
	callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));
}

void RelationshipController::HandleUnblockUser(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	RelationshipDTO dto;
	std::string error_msg;
	if (!GetDTOFromRequest(req, dto, error_msg))
	{
		callback(Utils::CreateErrorResponse(400, 400, error_msg));
		return;
	}

	if (dto.GetActionType() != RelationshipActionType::UnblockUser)
	{
		callback(Utils::CreateErrorResponse(400, 400, "error route request"));
		return;
	}

	//����Ϸ���
	if (!RelationshipService::IsBlocking(dto.GetActorUid(), dto.GetReactorUid()))
	{
		callback(Utils::CreateErrorResponse(400, 400, "can not unblock user"));
		return;
	}

	//�Ϸ���д���ϵ��
	std::string msg;
	if (!DatabaseManager::WriteRelationship(dto, msg))
	{
		LOG_ERROR << msg;
		callback(Utils::CreateErrorResponse(400, 400, "can not write into relationship table"));
		return;
	}
	//Ӧ�ý���֪ͨ


	Json::Value json_resp;
	json_resp["code"] = 200;
	json_resp["message"] = "success send request";
	callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));
}

void RelationshipController::HandleUnfriend(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	RelationshipDTO dto;
	std::string error_msg;
	if (!GetDTOFromRequest(req, dto, error_msg))
	{
		callback(Utils::CreateErrorResponse(400, 400, error_msg));
		return;
	}

	if (dto.GetActionType() != RelationshipActionType::Unfriend)
	{
		callback(Utils::CreateErrorResponse(400, 400, "error route request"));
		return;
	}

	//����Ϸ���
	if (!RelationshipService::IsFriend(dto.GetActorUid(), dto.GetReactorUid()))
	{
		callback(Utils::CreateErrorResponse(400, 400, "can not unfriend user"));
		return;
	}

	//�Ϸ���д���ϵ��
	std::string msg;
	if (!DatabaseManager::WriteRelationship(dto, msg))
	{
		LOG_ERROR << msg;
		callback(Utils::CreateErrorResponse(400, 400, "can not write into relationship table"));
		return;
	}
	//Ӧ�ý���֪ͨ


	Json::Value json_resp;
	json_resp["code"] = 200;
	json_resp["message"] = "success send request";
	callback(drogon::HttpResponse::newHttpJsonResponse(json_resp));
}

void RelationshipController::HandleGetFriendships(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	callback(drogon::HttpResponse::newHttpJsonResponse(DatabaseManager::GetRelationships()));
}

bool RelationshipController::ValidateBody(const drogon::HttpRequestPtr& req)
{
	const auto& body = req->getJsonObject();
	if (!body)
	{
		LOG_ERROR<<"body must be in json format";
		return false;
	}
	if (!body->isMember("sender_uid")||!body->isMember("receiver_uid"))
	{
		LOG_ERROR << "lack essential field";
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

bool RelationshipController::GetDTOFromRequest(const drogon::HttpRequestPtr& req, RelationshipDTO& dto, 
	std::string& error_msg)
{
	if (!ValidateBody(req))
	{
		error_msg = "request body is not valid";
		return false;
	}
	const auto& data = *(req->getJsonObject());
	if (!RelationshipDTO::BuildFromJson(data, dto))
	{
		error_msg = "missing fields";
		return false;
	}
	return true;
}