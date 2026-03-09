#pragma once

#include "Common/User.h"
#include <drogon/HttpController.h>

class IUserRepository
{
public:
	virtual ~IUserRepository() = default;
	virtual std::future<bool> AddUser(const UserInfo& info) = 0;
	virtual std::future<bool> ModifyUserInfo(const UserInfo& info) = 0;
	virtual std::future<bool> DeleteUser(const std::string& uid) = 0;
	virtual std::future<Json::Value> GetUserInfoByUid(const std::string& uid) const = 0;
	virtual std::future<Json::Value> GetUserInfoByAccount(const std::string& account) const = 0;
	virtual drogon::Task<UserInfo> GetUserInfo(const std::string& uid) = 0;

	// ─── 协程版本（供协程 Service 使用）───
	virtual drogon::Task<Json::Value> GetUserInfoByAccountCoro(const std::string& account) const = 0;
	virtual drogon::Task<Json::Value> GetUserInfoByUidCoro(const std::string& uid) const = 0;
	virtual drogon::Task<bool> AddUserCoro(const UserInfo& info) = 0;
	//static bool GetUserInfoByUid(const std::string& uid, Json::Value& data);
	//static bool GetUserInfoByAccount(const std::string& account, Json::Value& data);
	//static bool GetUserQueryInfoByAccount(const std::string& account, Json::Value& data);

	//static bool ValidateAccount(const std::string& account);
	//static bool ValidateUid(const std::string& uid);
	//static bool ValidateThreadId(unsigned thread_id);

};

