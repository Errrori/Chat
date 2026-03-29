#pragma once
#include "IUserRepository.h"
#include <drogon/orm/DbClient.h>

class PostgresUserRepository : public IUserRepository
{
public:
	explicit PostgresUserRepository(drogon::orm::DbClientPtr db) : _db(std::move(db)) {}

	drogon::Task<std::string> GetHashedPassword(const std::string& account) const override;
	drogon::Task<UserInfo> GetAuthUserByAccount(const std::string& account) const override;
	drogon::Task<UserInfo> GetDisplayProfileByUid(const std::string& uid) const override;
	drogon::Task<UserInfo> GetUserProfileByUid(const std::string& uid) const override;
	drogon::Task<UserInfo> FindUserByAccount(const std::string& account) const override;
	drogon::Task<AddUserStatus> AddUserCoro(const UserInfo& info) override;
	drogon::Task<bool> UpdateUserProfile(const std::string& uid, const UserInfo& update_info) override;

private:
	drogon::orm::DbClientPtr _db;
};

