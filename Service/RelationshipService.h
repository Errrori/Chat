#pragma once
#include <drogon/drogon.h>
#include <memory>
#include <vector>
#include <json/json.h>

class IRelationshipRepository;
class ConnectionService;
class UserService;

class RelationshipService
{
public:
	RelationshipService(std::shared_ptr<IRelationshipRepository> repo, std::shared_ptr<ConnectionService> connService);

	void SetUserService(std::shared_ptr<UserService> userService) { _user_service = std::move(userService); }

	drogon::Task<> SendFriendRequest(const std::string& requester_uid,
		const std::string& acceptor_uid, const std::string& message) const;
	drogon::Task<int64_t> ProcessFriendRequest(const std::string& requester_uid,
	                                           const std::string& acceptor_uid, int action) const;

	// Get unread notifications for a user
	drogon::Task<Json::Value> GetUnreadNotifications(const std::string& uid) const;

	// Get all notifications for a user (paginated)
	drogon::Task<Json::Value> GetNotifications(const std::string& uid, int offset, int limit) const;

	// Batch mark notifications as read; empty ids = mark all unread
	drogon::Task<size_t> MarkNotificationsRead(const std::string& uid, const std::vector<int64_t>& ids) const;

	// Get pending friend requests received by a user
	drogon::Task<Json::Value> GetPendingFriendRequests(const std::string& uid) const;

	// Block / Unblock
	drogon::Task<> BlockUser(const std::string& operator_uid, const std::string& blocked_uid) const;
	drogon::Task<> UnblockUser(const std::string& operator_uid, const std::string& blocked_uid) const;

	// Check block relationship (used by MessageService)
	drogon::Task<bool> IsBlocked(const std::string& uid_a, const std::string& uid_b) const;

private:
	std::shared_ptr<IRelationshipRepository> _relationship_repo;
	std::shared_ptr<ConnectionService> _conn_service;
	std::shared_ptr<UserService> _user_service;
};
