#include "pch.h"
#include "SQLiteRelationshipRepository.h"
#include "DbAccessor.h"
#include <json/json.h>

using namespace drogon::orm;

drogon::Task<> SQLiteRelationshipRepository::WriteFriendRequest(const std::string& requester_uid,
	const std::string& acceptor_uid, const std::string& payload)
{
	auto client = DbAccessor::GetDbClient();

	// 1. Transaction
	auto trans = co_await client->newTransactionCoro();

	try
	{
		// Insert into friend_requests
		// status is 0 (pending)
		co_await trans->execSqlCoro(
			"INSERT INTO friend_requests (requester_uid, target_uid, status) VALUES (?, ?, 0)",
			requester_uid, acceptor_uid);

		// Insert into relationship_event
		co_await trans->execSqlCoro(
			"INSERT INTO relationship_event (actor_uid, reactor_uid, type) VALUES (?, ?, 'RequestSent')",
			requester_uid, acceptor_uid);

		// Get the event ID
		auto idResult = co_await trans->execSqlCoro("SELECT last_insert_rowid()");
		if (idResult.size() == 0) throw std::runtime_error("Failed to get event id");
		auto eventId = idResult[0][0].as<int64_t>();

		// Insert into notifications
		co_await trans->execSqlCoro(
			"INSERT INTO notifications (event_id, type, sender_uid, recipient_uid, payload) VALUES (?, 0, ?, ?, ?)",
			eventId, requester_uid, acceptor_uid, payload);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Transaction failed in WriteFriendRequest: " << e.what();
		throw; 
	}
}

drogon::Task<int64_t> SQLiteRelationshipRepository::ProcessRequest(const std::string& requester_uid,
	const std::string& acceptor_id, int status, const std::string& payload)
{
	auto client = DbAccessor::GetDbClient();
	auto trans = co_await client->newTransactionCoro();
	int64_t threadId = -1;

	try
	{
		std::string eventType;
		if (status == 1) { // Accepted
			eventType = "RequestAccepted";
		}
		else if (status == 2) { // Refused
			eventType = "RequestRefused";
		}
		else {
			throw std::invalid_argument("Invalid status for processing request");
		}

		// Update Friend Request
		// Status: 0=pending, 1=accepted, 2=refused
		auto updateResult = co_await trans->execSqlCoro(
			"UPDATE friend_requests SET status = ?, updated_time = strftime('%s','now') WHERE requester_uid = ? AND target_uid = ? AND status = 0",
			status, requester_uid, acceptor_id);
		
		if (updateResult.affectedRows() == 0) {
			// Maybe it was already processed or doesn't exist
			LOG_WARN << "No pending request found to process between " << requester_uid << " and " << acceptor_id;
			co_return -1; 
		}

		// Insert Relationship Event
		co_await trans->execSqlCoro(
			"INSERT INTO relationship_event (actor_uid, reactor_uid, type) VALUES (?, ?, ?)",
			acceptor_id, requester_uid, eventType);

		if (status == 1) // Accepted
		{
			// Add Friendship
			// Ensure uid1 < uid2
			std::string uid1 = (requester_uid < acceptor_id) ? requester_uid : acceptor_id;
			std::string uid2 = (requester_uid < acceptor_id) ? acceptor_id : requester_uid;

			co_await trans->execSqlCoro(
				"INSERT INTO friendships (uid1, uid2) VALUES (?, ?)",
				uid1, uid2);

			// Create Thread (Private Chat)
			co_await trans->execSqlCoro(
				"INSERT INTO threads (type) VALUES (1)"); // 1 = private
			
			auto idResult = co_await trans->execSqlCoro("SELECT last_insert_rowid()");
			threadId = idResult[0][0].as<int64_t>();

			// Create Private Chat Entry
			co_await trans->execSqlCoro(
				"INSERT INTO private_chats (thread_id, uid1, uid2) VALUES (?, ?, ?)",
				threadId, uid1, uid2);
            
            // Send Notification for RequestAccepted (Type 1)
             // Event ID for this acceptance event
             auto evIdRes = co_await trans->execSqlCoro("SELECT last_insert_rowid()");
             auto evId = evIdRes[0][0].as<int64_t>();

             co_await trans->execSqlCoro(
                 "INSERT INTO notifications (event_id, type, sender_uid, recipient_uid, payload) VALUES (?, 1, ?, ?, ?)",
                 evId, acceptor_id, requester_uid, payload);
		}
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Transaction failed in ProcessRequest: " << e.what();
		throw;
	}
	co_return threadId;
}
