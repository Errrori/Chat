#pragma once
#include "DatabaseManager.h"

namespace Users
{
	bool AddUser(const std::string& name,const std::string& password,const std::string& uid);
	bool DeleteUser(const std::string& uid);
	bool FindUser(const std::string& uid);
	bool ModifyUserName(const std::string& uid, const std::string& name);
	bool ModifyUserPassword(const std::string& uid, const std::string& password);

	bool GetUserInfo(const std::string& uid, Json::Value& userInfo);
}