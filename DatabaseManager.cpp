#include "DatabaseManager.h"
using namespace DataBase;


void DatabaseManager::InitDatabase()
{
	drogon::app().getDbClient()->execSqlSync(USER_TABLE);
}

drogon::orm::DbClientPtr DatabaseManager::GetDbClient()
{
	static std::once_flag flag;
	std::call_once(flag, []
	{
		LOG_INFO << "Database init success";
		InitDatabase();
	});
	return drogon::app().getDbClient();
}
