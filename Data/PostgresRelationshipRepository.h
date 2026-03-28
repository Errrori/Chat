#pragma once
#include "Data/IRelationshipRepository.h"
#include <drogon/orm/DbClient.h>

class PostgresRelationshipRepository :public IRelationshipRepository
{
public:
	explicit PostgresRelationshipRepository(drogon::orm::DbClientPtr db) : _db(std::move(db)) {}
	drogon::Task<int64_t> WriteFriendRequest(const std::string& requester_uid,
		const std::string& acceptor_uid, const std::string& payload) override;

	drogon::Task<int64_t> ProcessRequest(const std::string& requester_uid,
	                                     const std::string& acceptor_uid, int status) override;

	drogon::Task<std::vector<drogon_model::postgres::Notifications>>
		GetUnreadNotifications(const std::string& uid) override;

	drogon::Task<std::vector<drogon_model::postgres::Notifications>>
		GetNotifications(const std::string& uid, int offset, int limit) override;

	drogon::Task<size_t>
		MarkNotificationsRead(const std::string& uid, const std::vector<int64_t>& ids) override;

	drogon::Task<std::vector<drogon_model::postgres::FriendRequests>>
		GetPendingFriendRequests(const std::string& uid) override;

	drogon::Task<> BlockUser(const std::string& operator_uid, const std::string& blocked_uid) override;
	drogon::Task<> UnblockUser(const std::string& operator_uid, const std::string& blocked_uid) override;
	drogon::Task<bool> IsBlocked(const std::string& uid_a, const std::string& uid_b) override;
	drogon::Task<bool> HasBlocked(const std::string& operator_uid, const std::string& target_uid) override;

private:
	drogon::orm::DbClientPtr _db;
};
