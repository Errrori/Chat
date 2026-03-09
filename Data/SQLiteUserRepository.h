#pragma once
#include "IUserRepository.h"
#include <drogon/orm/DbClient.h>

class SQLiteUserRepository:public IUserRepository
{
public:
	explicit SQLiteUserRepository(drogon::orm::DbClientPtr db) : _db(std::move(db)) {}

	std::future<bool> AddUser(const UserInfo& info) override;
	std::future<bool> ModifyUserInfo(const UserInfo& info) override;
	std::future<bool> DeleteUser(const std::string& uid) override;
	std::future<Json::Value> GetUserInfoByUid(const std::string& uid) const override;
	std::future<Json::Value> GetUserInfoByAccount(const std::string& account) const override;
	drogon::Task<UserInfo> GetUserInfo(const std::string& uid) override;

	// 协程版本
	drogon::Task<Json::Value> GetUserInfoByAccountCoro(const std::string& account) const override;
	drogon::Task<Json::Value> GetUserInfoByUidCoro(const std::string& uid) const override;
	drogon::Task<bool> AddUserCoro(const UserInfo& info) override;

private:
	drogon::orm::DbClientPtr _db;
};

