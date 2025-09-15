#pragma once
#include <drogon/drogon.h>
#include "const.h"
#include "MessageManager.h"
#include "models/Users.h"

class DatabaseManager
{
public:
	DatabaseManager() = delete;

	static void InitDatabase();
	static drogon::orm::DbClientPtr GetDbClient();

	static bool PushUser(const Json::Value& user_info);
	static Json::Value GetAllUsersInfo();
	static Json::Value GetMessages(unsigned thread_id, int64_t existing_id, unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static Json::Value GetAllMessage(unsigned thread_id, unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static bool GetUserInfoByUid(const std::string& uid, Json::Value& data);
	static bool GetUserInfoByAccount(const std::string& account, Json::Value& data);
	static bool GetUserQueryInfoByAccount(const std::string& account, Json::Value& data);

	static bool ModifyAvatar(const std::string& uid, const std::string& avatar);
	static bool ModifyUsername(const std::string& uid, const std::string& username);
	static bool ModifyPassword(const std::string& uid, const std::string& password);
	static bool ModifyUserInfo(const Utils::UserInfo& info);

	static bool DeleteUser(const std::string& uid);
	static bool ValidateAccount(const std::string& account);
	static bool ValidateUid(const std::string& uid);
	static bool ValidateThreadId(unsigned thread_id);

	static bool PushMessage(const MessageManager::MsgData& msg);

	static void CreatePublicThread();
	static void CreateDefaultUser();
};

