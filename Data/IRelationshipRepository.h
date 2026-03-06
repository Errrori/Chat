#pragma once
#include <drogon/drogon.h>
#include "models/Notifications.h"
#include "models/FriendRequests.h"

class IRelationshipRepository
{
public:
	virtual ~IRelationshipRepository() = default;

	virtual drogon::Task<int64_t>
		WriteFriendRequest(const std::string& requester_uid,const std::string& acceptor_uid,const std::string& payload) = 0;

	virtual drogon::Task<int64_t>
		ProcessRequest(const std::string& requester_uid, const std::string& acceptor_uid, int status) = 0;

	virtual drogon::Task<>
		WriteNotice(const std::string& actor_uid,const std::string& reactor_uid,int type,const std::string& payload) = 0;

	// Get unread notifications for a user (is_read == 0)
	virtual drogon::Task<std::vector<drogon_model::sqlite3::Notifications>>
		GetUnreadNotifications(const std::string& uid) = 0;

	// Get all notifications for a user with pagination
	virtual drogon::Task<std::vector<drogon_model::sqlite3::Notifications>>
		GetNotifications(const std::string& uid, int offset, int limit) = 0;

	// Batch mark notifications as read; empty ids = mark all unread; returns affected rows
	virtual drogon::Task<size_t>
		MarkNotificationsRead(const std::string& uid, const std::vector<int64_t>& ids) = 0;

	// Get pending friend requests received by a user
	virtual drogon::Task<std::vector<drogon_model::sqlite3::FriendRequests>>
		GetPendingFriendRequests(const std::string& uid) = 0;

	// Block / Unblock
	virtual drogon::Task<> BlockUser(const std::string& operator_uid, const std::string& blocked_uid) = 0;
	virtual drogon::Task<> UnblockUser(const std::string& operator_uid, const std::string& blocked_uid) = 0;

	// Check if either direction has a block relationship
	virtual drogon::Task<bool> IsBlocked(const std::string& uid_a, const std::string& uid_b) = 0;
	// Check if operator has blocked target (single direction)
	virtual drogon::Task<bool> HasBlocked(const std::string& operator_uid, const std::string& target_uid) = 0;
};
