#pragma once

#include "Common/User.h"
#include <drogon/HttpController.h>

class IUserRepository;

using RespCallback = std::function<void(const drogon::HttpResponsePtr&)>;
class UserService
{
	friend class Container;
public:

	UserService(std::shared_ptr<IUserRepository> ptr): _user_repo(std::move(ptr)) {}
	void UserRegister(const UserInfo& info, RespCallback&& callback) const;
	void UserLogin(const UserInfo& info, RespCallback&& callback) const;
	drogon::Task<UserInfo> GetUserInfo(const std::string& uid);

private:  // 改为 protected

	std::shared_ptr<IUserRepository> _user_repo;
};