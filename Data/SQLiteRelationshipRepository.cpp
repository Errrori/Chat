#include "pch.h"
#include "SQLiteRelationshipRepository.h"
#include "DbAccessor.h"

#include "models/FriendRequests.h"
#include "models/Friendships.h"
#include "models/Notifications.h"
#include "models/Threads.h"
#include "models/PrivateChats.h"
#include "models/Block.h"
#include "Common/Enums.h"

using namespace drogon::orm;
using namespace ChatEnums;
using FriendRequests = drogon_model::sqlite3::FriendRequests;
using Notifications = drogon_model::sqlite3::Notifications;
using Friendships = drogon_model::sqlite3::Friendships;
using Threads = drogon_model::sqlite3::Threads;
using PrivateChats = drogon_model::sqlite3::PrivateChats;
using Block = drogon_model::sqlite3::Block;

drogon::Task<int64_t> SQLiteRelationshipRepository::WriteFriendRequest(const std::string& requester_uid,
	const std::string& acceptor_uid, const std::string& payload)
{
	try
	{
		auto trans = co_await DbAccessor::GetDbClient()->newTransactionCoro();
		if (trans == nullptr)
			throw std::runtime_error("fail to create transaction");

		CoroMapper<Friendships> friendship_mapper(trans);
		std::string uid1 = (requester_uid < acceptor_uid) ? requester_uid : acceptor_uid;
		std::string uid2 = (requester_uid < acceptor_uid) ? acceptor_uid : requester_uid;
		Criteria criteria(Criteria(Friendships::Cols::_uid1,CompareOperator::EQ,uid1)
			&&Criteria(Friendships::Cols::_uid2,CompareOperator::EQ,uid2));
		const auto& result = co_await friendship_mapper.limit(1).findBy(criteria);
		if (!result.empty())
			throw std::invalid_argument("You are already friends with this user");

		// Check if either user has blocked the other
		CoroMapper<Block> block_mapper(trans);
		auto block_result = co_await block_mapper.limit(1).findBy(
			(Criteria(Block::Cols::_operator_uid, CompareOperator::EQ, requester_uid) &&
			 Criteria(Block::Cols::_blocked_uid, CompareOperator::EQ, acceptor_uid)) ||
			(Criteria(Block::Cols::_operator_uid, CompareOperator::EQ, acceptor_uid) &&
			 Criteria(Block::Cols::_blocked_uid, CompareOperator::EQ, requester_uid))
		);
		if (!block_result.empty())
			throw std::invalid_argument("Cannot send friend request due to block relationship");

		CoroMapper<FriendRequests> req_mapper(trans);
		//check if user has already sent a request before
		const auto& record = co_await req_mapper.limit(1).
			findBy(Criteria(Criteria(FriendRequests::Cols::_requester_uid, CompareOperator::EQ, requester_uid)
				&& Criteria(FriendRequests::Cols::_target_uid, CompareOperator::EQ, acceptor_uid)));
		if (!record.empty() && record[0].getValueOfStatus() != static_cast<int>(FriendRequestStatus::Refused))
			throw std::runtime_error("A pending request already exists or was accepted");

		try
		{
			FriendRequests new_req;
			new_req.setRequesterUid(requester_uid);
			new_req.setTargetUid(acceptor_uid);
			new_req.setStatus(static_cast<int>(FriendRequestStatus::Pending));
			new_req.setPayload(payload);
			co_await req_mapper.insert(new_req);

			Notifications notice;
			notice.setSenderUid(requester_uid);
			notice.setRecipientUid(acceptor_uid);
			notice.setType(static_cast<int>(NoticeType::RequestReceived));
			notice.setPayload(payload);

			CoroMapper<Notifications> notice_mapper(trans);
			const auto& result = co_await notice_mapper.insert(notice);
			co_return result.getValueOfId();
		}catch (const std::exception& e)
		{
			trans->rollback();
			throw e;
		}
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Transaction failed in WriteFriendRequest: " << e.what();
		throw; 
	}
}

namespace
{
    drogon::Task<int64_t> handleAcceptRequest(
        const std::shared_ptr<drogon::orm::Transaction>& trans,
        const std::string& requester_uid,
        const std::string& acceptor_uid,
        const drogon::orm::Criteria& criteria)
    {
        CoroMapper<FriendRequests> request_mapper(trans);
        CoroMapper<Friendships> friendship_mapper(trans);
        CoroMapper<Threads> thread_mapper(trans);
        CoroMapper<PrivateChats> private_chat_mapper(trans);

        co_await request_mapper.updateBy({ FriendRequests::Cols::_status },
            criteria, static_cast<int>(FriendRequestStatus::Accepted));

        // Add Friendship
        std::string uid1 = (requester_uid < acceptor_uid) ? requester_uid : acceptor_uid;
        std::string uid2 = (requester_uid < acceptor_uid) ? acceptor_uid : requester_uid;
        Friendships friendship;
        friendship.setUid1(uid1);
        friendship.setUid2(uid2);
        co_await friendship_mapper.insert(friendship);

        // Create Thread (Private Chat)
        Threads new_thread;
        new_thread.setType(1); // 1 = private
        const auto& thread_result = co_await thread_mapper.insert(new_thread);
        int64_t threadId = thread_result.getValueOfThreadId();

        // Create Private Chat Entry
        PrivateChats private_chat;
        private_chat.setThreadId(threadId);
        private_chat.setUid1(uid1);
        private_chat.setUid2(uid2);
        co_await private_chat_mapper.insert(private_chat);

        co_return threadId;
    }

    drogon::Task<> handleRefuseRequest(
        const std::shared_ptr<drogon::orm::Transaction>& trans,
        const drogon::orm::Criteria& criteria)
    {
        CoroMapper<FriendRequests> request_mapper(trans);
        co_await request_mapper.updateBy({ FriendRequests::Cols::_status },
            criteria, static_cast<int>(FriendRequestStatus::Refused));
        co_return;
    }
} // anonymous namespace

drogon::Task<int64_t> SQLiteRelationshipRepository::ProcessRequest(const std::string& requester_uid,
                                                                   const std::string& acceptor_uid, int status)
{
    auto trans = co_await DbAccessor::GetDbClient()->newTransactionCoro();
    if (!trans)
        throw std::runtime_error("Failed to create transaction");

    int64_t threadId = -1;

    try
    {
        CoroMapper<FriendRequests> request_mapper(trans);
        Criteria criteria(Criteria(FriendRequests::Cols::_requester_uid, CompareOperator::EQ, requester_uid) &&
            Criteria(FriendRequests::Cols::_target_uid, CompareOperator::EQ, acceptor_uid) &&
            Criteria(FriendRequests::Cols::_status, CompareOperator::EQ, static_cast<int>(FriendRequestStatus::Pending)));
        
        auto request = co_await request_mapper.limit(1).findBy(criteria);
        if (request.empty())
        {
            LOG_WARN << "No pending request found to process between " << requester_uid << " and " << acceptor_uid;
            co_return -1;
        }

        // Check block relationship before accepting
        if (status == static_cast<int>(FriendRequestStatus::Accepted))
        {
            CoroMapper<Block> block_mapper(trans);
            auto block_result = co_await block_mapper.limit(1).findBy(
                (Criteria(Block::Cols::_operator_uid, CompareOperator::EQ, requester_uid) &&
                 Criteria(Block::Cols::_blocked_uid, CompareOperator::EQ, acceptor_uid)) ||
                (Criteria(Block::Cols::_operator_uid, CompareOperator::EQ, acceptor_uid) &&
                 Criteria(Block::Cols::_blocked_uid, CompareOperator::EQ, requester_uid))
            );
            if (!block_result.empty())
                throw std::invalid_argument("Cannot process friend request due to block relationship");

            threadId = co_await handleAcceptRequest(trans, requester_uid, acceptor_uid, criteria);
        }
        else if (status == static_cast<int>(FriendRequestStatus::Refused))
        {
            co_await handleRefuseRequest(trans, criteria);
            threadId = 0; // Success, but no thread created
        }

        // Insert Notification
        CoroMapper<Notifications> notice_mapper(trans);
        Notifications notification;
        notification.setType(status);
        notification.setSenderUid(acceptor_uid);
        notification.setRecipientUid(requester_uid);
        co_await notice_mapper.insert(notification);
    }
    catch (const std::exception& e)
    {
        trans->rollback();
        LOG_ERROR << "Transaction failed in ProcessRequest: " << e.what();
        throw;
    }

    co_return threadId;
}

drogon::Task<> SQLiteRelationshipRepository::WriteNotice(const std::string& actor_uid, const std::string& reactor_uid,
	int type, const std::string& payload)
{
	Notifications notice;
	notice.setSenderUid(actor_uid);
	notice.setRecipientUid(reactor_uid);
	notice.setType(type);
	notice.setPayload(payload);

	CoroMapper<Notifications> mapper(DbAccessor::GetDbClient());
	try
	{
		const auto& result = co_await mapper.insert(notice);
	}
	catch (const std::exception& e)
	{
		throw e;
	}
}

drogon::Task<std::vector<Notifications>> SQLiteRelationshipRepository::GetUnreadNotifications(
	const std::string& uid)
{
	auto client = DbAccessor::GetDbClient();
	auto result = co_await client->execSqlCoro(
		"SELECT n.* FROM notifications n "
		"LEFT JOIN block b ON (b.operator_uid = ? AND b.blocked_uid = n.sender_uid) "
		"WHERE n.recipient_uid = ? AND n.is_read = 0 AND b.operator_uid IS NULL "
		"ORDER BY n.created_time DESC",
		uid, uid
	);
	std::vector<Notifications> items;
	items.reserve(result.size());
	for (const auto& row : result)
		items.emplace_back(row, -1);
	co_return items;
}

drogon::Task<std::vector<Notifications>> SQLiteRelationshipRepository::GetNotifications(
	const std::string& uid, int offset, int limit)
{
	auto client = DbAccessor::GetDbClient();
	auto result = co_await client->execSqlCoro(
		"SELECT n.* FROM notifications n "
		"LEFT JOIN block b ON (b.operator_uid = ? AND b.blocked_uid = n.sender_uid) "
		"WHERE n.recipient_uid = ? AND b.operator_uid IS NULL "
		"ORDER BY n.created_time DESC LIMIT ? OFFSET ?",
		uid, uid, limit, offset
	);
	std::vector<Notifications> items;
	items.reserve(result.size());
	for (const auto& row : result)
		items.emplace_back(row, -1);
	co_return items;
}

drogon::Task<size_t> SQLiteRelationshipRepository::MarkNotificationsRead(
	const std::string& uid, const std::vector<int64_t>& ids)
{
	auto client = DbAccessor::GetDbClient();
	drogon::orm::Result result(nullptr);

	if (ids.empty())
	{
		// Mark all unread notifications for this user
		result = co_await client->execSqlCoro(
			"UPDATE notifications SET is_read = 1 WHERE recipient_uid = ? AND is_read = 0",
			uid
		);
	}
	else
	{
			// ids are int64_t -> to_string, no injection risk
		std::string id_list;
		id_list.reserve(ids.size() * 8);
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (i > 0) id_list += ',';
			id_list += std::to_string(ids[i]);
		}
		std::string sql = "UPDATE notifications SET is_read = 1 WHERE recipient_uid = ? AND id IN (" + id_list + ")";
		result = co_await client->execSqlCoro(sql, uid);
	}

	co_return static_cast<size_t>(result.affectedRows());
}

drogon::Task<std::vector<FriendRequests>> SQLiteRelationshipRepository::GetPendingFriendRequests(
	const std::string& uid)
{
	CoroMapper<FriendRequests> mapper(DbAccessor::GetDbClient());
	auto result = co_await mapper
		.orderBy(FriendRequests::Cols::_created_time, SortOrder::DESC)
		.findBy(
			Criteria(FriendRequests::Cols::_target_uid, CompareOperator::EQ, uid) &&
			Criteria(FriendRequests::Cols::_status, CompareOperator::EQ,
				static_cast<int>(ChatEnums::FriendRequestStatus::Pending))
		);
	co_return result;
}

drogon::Task<> SQLiteRelationshipRepository::BlockUser(const std::string& operator_uid, const std::string& blocked_uid)
{
	bool already = co_await HasBlocked(operator_uid, blocked_uid);
	if (already)
		throw std::invalid_argument("User is already blocked");

	auto client = DbAccessor::GetDbClient();
	co_await client->execSqlCoro(
		"INSERT INTO block (operator_uid, blocked_uid) VALUES (?, ?)",
		operator_uid, blocked_uid
	);
}

drogon::Task<> SQLiteRelationshipRepository::UnblockUser(const std::string& operator_uid, const std::string& blocked_uid)
{
	bool exists = co_await HasBlocked(operator_uid, blocked_uid);
	if (!exists)
		throw std::invalid_argument("No block relationship found");

	auto client = DbAccessor::GetDbClient();
	co_await client->execSqlCoro(
		"DELETE FROM block WHERE operator_uid = ? AND blocked_uid = ?",
		operator_uid, blocked_uid
	);
}

drogon::Task<bool> SQLiteRelationshipRepository::IsBlocked(const std::string& uid_a, const std::string& uid_b)
{
	CoroMapper<Block> mapper(DbAccessor::GetDbClient());
	auto result = co_await mapper.limit(1).findBy(
		(Criteria(Block::Cols::_operator_uid, CompareOperator::EQ, uid_a) &&
		 Criteria(Block::Cols::_blocked_uid, CompareOperator::EQ, uid_b)) ||
		(Criteria(Block::Cols::_operator_uid, CompareOperator::EQ, uid_b) &&
		 Criteria(Block::Cols::_blocked_uid, CompareOperator::EQ, uid_a))
	);
	co_return !result.empty();
}

drogon::Task<bool> SQLiteRelationshipRepository::HasBlocked(const std::string& operator_uid, const std::string& target_uid)
{
	CoroMapper<Block> mapper(DbAccessor::GetDbClient());
	auto result = co_await mapper.limit(1).findBy(
		Criteria(Block::Cols::_operator_uid, CompareOperator::EQ, operator_uid) &&
		Criteria(Block::Cols::_blocked_uid, CompareOperator::EQ, target_uid)
	);
	co_return !result.empty();
}
