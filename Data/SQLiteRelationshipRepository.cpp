#include "pch.h"
#include "SQLiteRelationshipRepository.h"
#include "DbAccessor.h"

#include "models/FriendRequests.h"
#include "models/Friendships.h"
#include "models/Notifications.h"
#include "models/Threads.h"
#include "models/PrivateChats.h"
#include "Common/Enums.h"

using namespace drogon::orm;
using namespace ChatEnums;
using FriendRequests = drogon_model::sqlite3::FriendRequests;
using Notifications = drogon_model::sqlite3::Notifications;
using Friendships = drogon_model::sqlite3::Friendships;
using Threads = drogon_model::sqlite3::Threads;
using PrivateChats = drogon_model::sqlite3::PrivateChats;

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

drogon::Task<int64_t> SQLiteRelationshipRepository::ProcessRequest(const std::string& requester_uid,
                                                                   const std::string& acceptor_uid, int status)
{
	auto trans = co_await DbAccessor::GetDbClient()->newTransactionCoro();
	if (!trans)
		throw std::runtime_error("Failed to create transaction");

	int64_t threadId = -1;

	try
	{
		// Find the pending friend request
		CoroMapper<FriendRequests> request_mapper(trans);
		Criteria criteria(Criteria(FriendRequests::Cols::_requester_uid, CompareOperator::EQ, requester_uid) &&
			Criteria(FriendRequests::Cols::_target_uid, CompareOperator::EQ, acceptor_uid) &&
			Criteria(FriendRequests::Cols::_status, CompareOperator::EQ, static_cast<int>(FriendRequestStatus::Pending)));
		auto request = co_await request_mapper.limit(1).findBy(criteria);

		if (request.empty()) {
			LOG_WARN << "No pending request found to process between " << requester_uid << " and " << acceptor_uid;
			co_return -1;
		}

		if (status == static_cast<int>(FriendRequestStatus::Accepted)) // Accepted
		{
			co_await request_mapper.updateBy({ FriendRequests::Cols::_status },
				criteria, static_cast<int>(FriendRequestStatus::Accepted));

			// Add Friendship
			std::string uid1 = (requester_uid < acceptor_uid) ? requester_uid : acceptor_uid;
			std::string uid2 = (requester_uid < acceptor_uid) ? acceptor_uid : requester_uid;
			Friendships friendship;
			friendship.setUid1(uid1);
			friendship.setUid2(uid2);
			CoroMapper<Friendships> friendship_mapper(trans);
			co_await friendship_mapper.insert(friendship);

			// Create Thread (Private Chat)
			Threads new_thread;
			new_thread.setType(1); // 1 = private
			CoroMapper<Threads> thread_mapper(trans);
			const auto& thread_result = co_await thread_mapper.insert(new_thread);
			threadId = thread_result.getValueOfThreadId();

			// Create Private Chat Entry
			PrivateChats private_chat;
			private_chat.setThreadId(threadId);
			private_chat.setUid1(uid1);
			private_chat.setUid2(uid2);
			CoroMapper<PrivateChats> private_chat_mapper(trans);
			co_await private_chat_mapper.insert(private_chat);

		}
		else if (status == static_cast<int>(FriendRequestStatus::Refused))
		{
			co_await request_mapper.updateBy({ FriendRequests::Cols::_status }, 
				criteria, static_cast<int>(FriendRequestStatus::Refused));
			threadId = 0; // No thread created for rejected requests, but 0 indicates success
		}

		// Insert Notification
		Notifications notification;
		notification.setType(status); // 1=RequestAccepted, 2=RequestRejected, matches NoticeType enum
		notification.setSenderUid(acceptor_uid);
		notification.setRecipientUid(requester_uid);
		CoroMapper<Notifications> notice_mapper(trans);
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
