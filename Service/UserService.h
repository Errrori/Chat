#pragma once

#include "Common/User.h"
#include <drogon/HttpController.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/utils/coroutine.h>

class IUserRepository;
class RedisService;

class UserService
{
public:
	UserService(std::shared_ptr<IUserRepository> ptr, std::shared_ptr<RedisService> redis)
		: _user_repo(std::move(ptr)), _redis_service(std::move(redis)) {}
	drogon::Task<drogon::HttpResponsePtr> UserRegister(const UserInfo& info) const;
	drogon::Task<drogon::HttpResponsePtr> UserLogin(const UserInfo& info) const;
	drogon::Task<UserInfo> GetDisplayProfileByUid(const std::string& uid) const;
	drogon::Task<drogon::HttpResponsePtr> RefreshToken(const std::string& refresh_token) const;
	drogon::Task<drogon::HttpResponsePtr> Logout(const std::string& refresh_token) const;
	drogon::Task<UserInfo> GetUserProfileByUid(const std::string& uid) const;
	drogon::Task<UserInfo> SearchUserByAccount(const std::string& account) const;
	drogon::Task<UserInfo> UpdateUserProfile(
		const std::string& uid, const UserInfo& update_info) const;
private:
	std::shared_ptr<IUserRepository> _user_repo;
	std::shared_ptr<RedisService> _redis_service;
};