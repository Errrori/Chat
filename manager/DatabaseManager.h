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

	static void PushChatRecords(const Json::Value& message);
	static Json::Value GetChatRecords(int64_t existing_id,unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static Json::Value GetAllRecords(unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static bool ModifyAvatar(const std::string& uid, const std::string& avatar);
private:
	static Json::Value WriteRecordsReserveOrder(const std::vector<drogon_model::sqlite3::ChatRecords>& records,
	                                            Json::Value& data);
};

//
//ADD_METHOD_TO(DbInfoController::GetDbInfo, "/db_info", drogon::Get, "CorsMiddleware");
//ADD_METHOD_TO(DbInfoController::ModifyName, "/modify_name", drogon::Post, "CorsMiddleware");
//ADD_METHOD_TO(DbInfoController::ModifyPassword, "/modify_password", drogon::Post, "CorsMiddleware");
//ADD_METHOD_TO(DbInfoController::DeleteUser, "/delete_user", drogon::Post, "CorsMiddleware");
//ADD_METHOD_TO(DbInfoController::GetUserById, "/get_user", drogon::Get, "CorsMiddleware");
//ADD_METHOD_TO(DbInfoController::ImportUsers, "/import", drogon::Post, "CorsMiddleware");
//ADD_METHOD_TO(DbInfoController::GetOnlineUsers, "/online_users", drogon::Get, "CorsMiddleware");
//ADD_METHOD_TO(DbInfoController::GetChatRecords, "/chatroom/record", drogon::Get, "CorsMiddleware");