#pragma once
#include "IUserRepository.h"
#include <drogon/orm/DbClient.h>

class SQLiteUserRepository : public IUserRepository
{
public:
	explicit SQLiteUserRepository(drogon::orm::DbClientPtr db) : _db(std::move(db)) {}

	drogon::Task<std::string> GetHashedPassword(const std::string& account) const override;
	drogon::Task<UserInfo>    GetUserByUid(const std::string& uid) const override;
	drogon::Task<UserInfo>    GetUserByAccount(const std::string& account) const override;
	drogon::Task<bool>        AddUserCoro(const UserInfo& info) override;

private:
	drogon::orm::DbClientPtr _db;
};

