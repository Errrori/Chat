#pragma once
#include "IUserRepository.h"

class SQLiteUserRepository:public IUserRepository
{
public:
	std::future<bool> AddUser(const UserInfo& info) override;
	std::future<bool> ModifyUserInfo(const UserInfo& info) override;
	std::future<bool> DeleteUser(const std::string& uid) override;
	std::future<Json::Value> GetUserInfoByUid(const std::string& uid) const override;
	std::future<Json::Value> GetUserInfoByAccount(const std::string& account) const override;
	drogon::Task<UserInfo> GetUserInfo(const std::string& uid) override;
};

