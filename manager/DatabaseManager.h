#pragma once
#include <drogon/drogon.h>
#include "const.h"
#include "models/Notifications.h"

namespace drogon_model::sqlite3
{
	class ChatRecords;
}

class DatabaseManager
{
public:
	DatabaseManager() = delete;

	static void InitDatabase();
	static drogon::orm::DbClientPtr GetDbClient();

	static bool PushUser(const Json::Value& user_info);
	static void PushChatRecords(const Json::Value& message);
	static void WriteFriendRequest(const Json::Value& request);
	static void PushNotification(const Json::Value& data);

	static Json::Value GetAllUsersInfo();
	static Json::Value GetChatRecords(int64_t existing_id,unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static Json::Value GetAllRecords(unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static bool GetUserInfoByUid(const std::string& uid, Json::Value& data);
	static bool GetUserInfoByAccount(const std::string& account, Json::Value& data);
	static bool GetUserNotification(const std::string& uid,Json::Value& data);


	static bool ModifyAvatar(const std::string& uid, const std::string& avatar);
	static bool ModifyUsername(const std::string& uid, const std::string& username);
	static bool ModifyPassword(const std::string& uid, const std::string& password);

	static bool DeleteUser(const std::string& uid);
	static bool ValidateAccount(const std::string& account);
	static bool ValidateUid(const std::string& uid);

	static std::vector<std::string> GetGroupMember(const std::string& group_id);
private:
	static Json::Value WriteRecord(const drogon_model::sqlite3::ChatRecords& record);
	static Json::Value WriteNotification(const drogon_model::sqlite3::Notifications& notification);
};

