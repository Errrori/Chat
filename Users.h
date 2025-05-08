#pragma once
#include "DatabaseManager.h"

namespace Users
{
	bool AddUser(const std::string& name,const std::string& password,const std::string& uid);
	bool DeleteUser(const std::string& uid);
	bool FindUserByUid(const std::string& uid);
	bool FindUserByName(const std::string& uid);
	bool ModifyUserName(const std::string& uid, const std::string& name);
	bool ModifyUserPassword(const std::string& uid, const std::string& password);
	bool GetUserInfoByName(const std::string& uid, Json::Value& userInfo);
	bool GetUserInfoByUid(const std::string& uid, Json::Value& userInfo);
}