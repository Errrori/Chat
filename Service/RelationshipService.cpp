#include "pch.h"
#include "RelationshipService.h"

#include "UserService.h"
#include "Data/IRelationshipRepository.h"
#include "Service/ConnectionService.h"
#include "Common/Notice.h"
#include "Utils.h"
#include "Common/Enums.h"

using namespace ChatEnums;
RelationshipService::RelationshipService(std::shared_ptr<IRelationshipRepository> repo, std::shared_ptr<ConnectionService> connService)
	: _relationship_repo(std::move(repo)), _conn_service(std::move(connService))
{
}

drogon::Task<> RelationshipService::SendFriendRequest(const std::string& requester_uid,
	const std::string& acceptor_uid, const std::string& message) const
{
	try
	{
		// 1. Validate acceptor exists

		auto event_id = co_await _relationship_repo->WriteFriendRequest(
			requester_uid, acceptor_uid, message);
		if (event_id < 0)
			throw std::runtime_error("Friend request already exists or you are already friends");
		auto user_info = co_await _user_service->GetDisplayProfileByUid(requester_uid);

		// 2. Build Notification Object
		Notice notice;
		notice.setSenderUid(requester_uid);
		notice.setSenderName(user_info.GetUsername().value_or(""));
		notice.setSenderAvatar(user_info.GetAvatar().value_or(""));
		notice.setMessage(message);
		notice.setCreatedTime(Utils::GetCurrentTimeStamp());
		notice.setNoticeId(event_id);
		notice.setType(NoticeType::RequestReceived);

		Json::Value json_notice = notice.ToJson();
		auto result = co_await _conn_service->DeliverToUser(
			acceptor_uid,
			ChatDelivery::OutboundMessage::Envelope(
				json_notice,
				ChatDelivery::DeliveryPolicy::MustDeliver,
				ChatDelivery::OfflineChannel::Notice));
		if (result.state == ChatDelivery::DeliveryState::Queued)
			LOG_INFO << "queued friend request notice for offline user: " << acceptor_uid;
	}catch (const std::exception& e)
	{
		throw;
	}

}

drogon::Task<int64_t> RelationshipService::ProcessFriendRequest(const std::string& requester_uid,
	const std::string& acceptor_uid, int status) const
{
	//this function must operated by the acceptor
    try
    {
		// When accepting a request, the sender of the notice is the one who accepted it.
		auto acceptor_info = co_await _user_service->GetDisplayProfileByUid(acceptor_uid);

		int64_t thread_id = co_await _relationship_repo->ProcessRequest(requester_uid, acceptor_uid, status);
		if (thread_id < 0)
			throw std::invalid_argument("can not process the request");

		Notice notice;
		notice.setSenderUid(acceptor_uid);
		notice.setSenderName(acceptor_info.GetUsername().value_or(""));
		notice.setSenderAvatar(acceptor_info.GetAvatar().value_or(""));
		notice.setCreatedTime(Utils::GetCurrentTimeStamp());
        if (status == static_cast<int>(FriendRequestStatus::Accepted)) // Accepted
            notice.setType(NoticeType::RequestAccepted);
        else if (status == static_cast<int>(FriendRequestStatus::Refused))
			notice.setType(NoticeType::RequestRejected);

		auto result = co_await _conn_service->DeliverToUser(
			requester_uid,
			ChatDelivery::OutboundMessage::Envelope(
				notice.ToJson(),
				ChatDelivery::DeliveryPolicy::MustDeliver,
				ChatDelivery::OfflineChannel::Notice));
		if (result.state == ChatDelivery::DeliveryState::Queued)
			LOG_INFO << "queued process-request notice for offline user: " << requester_uid;
    	
    	co_return thread_id;
    }
    catch (const std::exception& e)
    {
        // It's better to log the error and decide if we should re-throw
        LOG_ERROR << "Error while preparing acceptance notification: " << e.what();
		throw;
    }


}

drogon::Task<Json::Value> RelationshipService::GetUnreadNotifications(const std::string& uid) const
{
	auto items = co_await _relationship_repo->GetUnreadNotifications(uid);

	Json::Value data;
	data["items"] = Json::arrayValue;
	for (const auto& n : items)
		data["items"].append(n.toJson());
	data["count"] = static_cast<int>(items.size());
	co_return data;
}

drogon::Task<Json::Value> RelationshipService::GetNotifications(
	const std::string& uid, int offset, int limit) const
{
	auto items = co_await _relationship_repo->GetNotifications(uid, offset, limit);

	Json::Value data;
	data["items"] = Json::arrayValue;
	for (const auto& n : items)
		data["items"].append(n.toJson());
	data["count"]  = static_cast<int>(items.size());
	data["offset"] = offset;
	data["limit"]  = limit;
	co_return data;
}

drogon::Task<size_t> RelationshipService::MarkNotificationsRead(
	const std::string& uid, const std::vector<int64_t>& ids) const
{
	co_return co_await _relationship_repo->MarkNotificationsRead(uid, ids);
}

drogon::Task<Json::Value> RelationshipService::GetPendingFriendRequests(const std::string& uid) const
{
	auto items = co_await _relationship_repo->GetPendingFriendRequests(uid);

	Json::Value data;
	data["items"] = Json::arrayValue;
	for (const auto& req : items)
		data["items"].append(req.toJson());
	data["count"] = static_cast<int>(items.size());
	//do return auto increment id,it is not necessary
	for (int i = 0;i<data["count"].asInt();i++)
		data["items"][i].removeMember("id");
	co_return data;
}

drogon::Task<> RelationshipService::BlockUser(const std::string& operator_uid, const std::string& blocked_uid) const
{
	co_await _relationship_repo->BlockUser(operator_uid, blocked_uid);
}

drogon::Task<> RelationshipService::UnblockUser(const std::string& operator_uid, const std::string& blocked_uid) const
{
	co_await _relationship_repo->UnblockUser(operator_uid, blocked_uid);
}

drogon::Task<bool> RelationshipService::IsBlocked(const std::string& uid_a, const std::string& uid_b) const
{
	co_return co_await _relationship_repo->IsBlocked(uid_a, uid_b);
}
