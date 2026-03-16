#pragma once
#include "IUserRepository.h"
#include <drogon/orm/DbClient.h>

class SQLiteUserRepository : public IUserRepository
{
public:
	explicit SQLiteUserRepository(drogon::orm::DbClientPtr db) : _db(std::move(db)) {}

	drogon::Task<std::string> GetHashedPassword(const std::string& account) const override;
	drogon::Task<UserInfo> GetAuthUserByAccount(const std::string& account) const override;
	drogon::Task<UsersInfo> GetDisplayProfileByUid(const std::string& uid) const override;
	drogon::Task<UsersInfo> GetUserProfileByUid(const std::string& uid) const override;
	drogon::Task<UsersInfo> FindUserByAccount(const std::string& account) const override;
	drogon::Task<bool> AddUserCoro(const UserInfo& info) override;
	drogon::Task<bool> UpdateUserProfile(const std::string& uid, const UsersInfo& update_info) override;

private:
	drogon::orm::DbClientPtr _db;
};

