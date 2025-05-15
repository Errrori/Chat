#pragma once
#include <drogon/drogon.h>
#include "const.h"

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
	static Json::Value GetAllUsersInfo();
	static Json::Value GetChatRecords(int64_t existing_id,unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static Json::Value GetAllRecords(unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static bool GetUserInfoByUid(const std::string& uid, Json::Value& data);
	static bool GetUserInfoByUsername(const std::string& username, Json::Value& data);
	static bool ModifyAvatar(const std::string& uid, const std::string& avatar);
	static bool ModifyUsername(const std::string& uid, const std::string& username);
	static bool ModifyPassword(const std::string& uid, const std::string& password);
	static bool DeleteUser(const std::string& uid);

private:
	static Json::Value WriteRecordsReserveOrder(const std::vector<drogon_model::sqlite3::ChatRecords>& records,
	                                            Json::Value& data);
};

