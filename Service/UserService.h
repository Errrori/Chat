#pragma once

class IUserRepository;
class UserInfo;

using RespCallback = std::function<void(const drogon::HttpResponsePtr&)>;
class UserService
{
	friend class Container;
public:

	UserService(std::shared_ptr<IUserRepository> ptr): _user_repo(std::move(ptr)) {}
	void UserRegister(const UserInfo& info, RespCallback&& callback) const;
	void UserLogin(const UserInfo& info, RespCallback&& callback) const;

private:  // 改为 protected

	std::shared_ptr<IUserRepository> _user_repo;
};