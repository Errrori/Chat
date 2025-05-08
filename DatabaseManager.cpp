#include "DatabaseManager.h"
using namespace DataBase;


void DatabaseManager::InitDatabase()
{
	if (!is_init)
	{
		std::cout << "init database!\n";
		drogon::app().getDbClient()->execSqlSync(USER_TABLE);
		is_init = true;
	}
}

drogon::orm::DbClientPtr DatabaseManager::GetDbClient()
{
	if (!DataBase::is_init)
	{
		InitDatabase();
	}
	return drogon::app().getDbClient();
}
