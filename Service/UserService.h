#pragma once

#include "Common/User.h"
#include <drogon/HttpController.h>
#include <drogon/HttpAppFramework.h>

class IUserRepository;
class RedisService;

class UserService
{
public:
	struct GetUserResult
	{
		bool ok{false};
		int http_status{500};
		std::string message;
		UserInfo user;
	};

	struct ModifyUserResult
	{
		bool ok{false};
		int http_status{500};
		std::string message;
		Json::Value data;
	};

	UserService(std::shared_ptr<IUserRepository> ptr, std::shared_ptr<RedisService> redis)
		: _user_repo(std::move(ptr)), _redis_service(std::move(redis)) {}
	drogon::Task<drogon::HttpResponsePtr> UserRegister(const UserInfo& info) const;
	drogon::Task<drogon::HttpResponsePtr> UserLogin(const UserInfo& info) const;
	drogon::Task<UserInfo> GetUserInfo(const std::string& uid);
	drogon::Task<drogon::HttpResponsePtr> RefreshToken(const std::string& refresh_token) const;
	drogon::Task<drogon::HttpResponsePtr> Logout(const std::string& refresh_token) const;
	drogon::Task<GetUserResult> GetUser(
		std::optional<std::string> uid, std::optional<std::string> account) const;
	drogon::Task<ModifyUserResult> ModifyUserInfo(const Json::Value& body) const;
private:
	std::shared_ptr<IUserRepository> _user_repo;
	std::shared_ptr<RedisService> _redis_service;
};