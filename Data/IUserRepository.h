#pragma once

#include "Common/User.h"
#include <drogon/HttpController.h>

class IUserRepository
{
public:
	virtual ~IUserRepository() = default;

	// Returns hashed password for the given account (login use only)
	virtual drogon::Task<std::string> GetHashedPassword(const std::string& account) const = 0;

	// Returns user public info (uid/username/account/avatar) by uid or account
	virtual drogon::Task<UserInfo> GetUserByUid(const std::string& uid) const = 0;
	virtual drogon::Task<UserInfo> GetUserByAccount(const std::string& account) const = 0;

	// Persists a new user (requires hashed_password set on info)
	virtual drogon::Task<bool> AddUserCoro(const UserInfo& info) = 0;
};

