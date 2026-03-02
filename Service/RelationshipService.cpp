#include "pch.h"
#include "RelationshipService.h"
#include "Data/IRelationshipRepository.h"
#include "Service/ConnectionService.h"
#include "Data/DbAccessor.h"
#include "Common/Notice.h"
#include "Utils.h"

RelationshipService::RelationshipService(std::shared_ptr<IRelationshipRepository> repo, std::shared_ptr<ConnectionService> connService)
	: _relationship_repo(std::move(repo)), _conn_service(std::move(connService))
{
}

drogon::Task<> RelationshipService::SendFriendRequest(const std::string& requester_uid, const std::string& acceptor_uid, const std::string& message)
{
	// 1. Get Requester Info for Notification Payload
	auto client = DbAccessor::GetDbClient();
	auto result = co_await client->execSqlCoro("SELECT username, avatar FROM users WHERE uid = ?", requester_uid);
	if (result.size() == 0)
	{
		LOG_ERROR << "Requester not found: " << requester_uid;
		throw std::runtime_error("Requester not found");
	}

	std::string username = result[0]["username"].as<std::string>();
	std::string avatar = result[0]["avatar"].as<std::string>();

	// 2. Build Notification Object
	Notice notice;
	notice.setSenderUid(requester_uid);
	notice.setSenderName(username);
	notice.setSenderAvatar(avatar);
	notice.setMessage(message);
	notice.setCreatedTime(Utils::GetCurrentTimeStamp());

	// Convert to JSON for DB and Websocket
	// For DB, we usually store the inner data or the full structure?
	// Based on "notification payload" in previous context, it likely expects just the data part or a specific structure.
	// But the user requested "Notice { type: 0, data: {...} }" format for realtime push.
	// For DB storage "payload" column, usually we store the data part to be flexible, or the whole thing.
	// Let's store the whole JSON for consistency if the frontend expects to parse it similarly from history.
	// However, usually DB payload is just the content. 
	// Let's stick to the user's new requirement: "Notice { type: 0, data: ... }"
	
	Json::Value noticeJson = notice.ToJson();
	Json::FastWriter writer;
	std::string payloadStr = writer.write(noticeJson);

	// 3. Persist to DB
	// Note: We are storing the full JSON { type: 0, data: ... } into the payload column.
	co_await _relationship_repo->WriteFriendRequest(requester_uid, acceptor_uid, payloadStr);

	// 4. Send Real-time Notification
	_conn_service->PostNotice(acceptor_uid, noticeJson);
}

drogon::Task<int64_t> RelationshipService::ProcessFriendRequest(const std::string& requester_uid, const std::string& acceptor_id, int status)
{
	std::string payloadStr = "";
	Json::Value noticeJson;

	if (status == 1) // Accepted
	{
		// Get Acceptor Info (who accepted the request)
		auto client = DbAccessor::GetDbClient();
		auto result = co_await client->execSqlCoro("SELECT username, avatar FROM users WHERE uid = ?", acceptor_id);
		
		std::string username = "";
		std::string avatar = "";
		
		if (result.size() > 0) {
			username = result[0]["username"].as<std::string>();
			avatar = result[0]["avatar"].as<std::string>();
		}

		Notice notice;
		notice.setSenderUid(acceptor_id);
		notice.setSenderName(username);
		notice.setSenderAvatar(avatar);
		notice.setMessage("Friend request accepted");
		notice.setCreatedTime(Utils::GetCurrentTimeStamp());

		noticeJson = notice.ToJson();
		Json::FastWriter writer;
		payloadStr = writer.write(noticeJson);
	}

	// Persist
	int64_t thread_id = co_await _relationship_repo->ProcessRequest(requester_uid, acceptor_id, status, payloadStr);

	// Notify Requester if Accepted
	if (status == 1)
	{
		std::vector<std::string> targets = { requester_uid };
		_conn_service->Broadcast(targets, noticeJson);
	}

	co_return thread_id;
}
