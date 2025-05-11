#pragma once
#include <drogon/drogon.h>

namespace DataBase
{
	static bool is_init = false;

	const static std::string USER_TABLE = "CREATE TABLE IF NOT EXISTS users("
										  "id INTEGER PRIMARY KEY AUTOINCREMENT,"
										  "username TEXT NOT NULL,"
										  "password TEXT NOT NULL,"
										  "uid TEXT NOT NULL UNIQUE,"
										  "avatar TEXT DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
										  "create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
										  ");";
	const static std::string CHAT_RECORDS_TABLE = "CREATE TABLE IF NOT EXISTS chat_records("
										  "message_id BIGINT PRIMARY KEY,"
										  "sender_name TEXT NOT NULL,"
										  "sender_uid TEXT NOT NULL,"
										  "content TEXT NOT NULL,"
										  "avatar TEXT DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
										  "create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
										  ");";
	constexpr static unsigned int DEFAULT_RECORDS_QUERY_LEN = 50;
}


class DatabaseManager
{
public:
	DatabaseManager() = delete;

	static void InitDatabase();
	static drogon::orm::DbClientPtr GetDbClient();

	static void PushChatRecords(const Json::Value& message);
	static Json::Value GetChatRecords(unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
};

