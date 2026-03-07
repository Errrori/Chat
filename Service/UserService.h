#pragma once

#include "Common/User.h"
#include <drogon/HttpController.h>

class IUserRepository;
class RedisService;

using RespCallback = std::function<void(const drogon::HttpResponsePtr&)>;
class UserService
{
	friend class Container;
public:

	UserService(std::shared_ptr<IUserRepository> ptr, std::shared_ptr<RedisService> redis)
		: _user_repo(std::move(ptr)), _redis_service(std::move(redis)) {}
	void UserRegister(const UserInfo& info, RespCallback&& callback) const;
	void UserLogin(const UserInfo& info, RespCallback&& callback) const;
	drogon::Task<UserInfo> GetUserInfo(const std::string& uid);

private:

	std::shared_ptr<IUserRepository> _user_repo;
	std::shared_ptr<RedisService> _redis_service;
};