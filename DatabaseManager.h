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
										  "create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
										  ");";
}

class DatabaseManager
{
public:
	DatabaseManager() = delete;

	static void InitDatabase();
	static drogon::orm::DbClientPtr GetDbClient();
};

