#pragma once

class DbAccessor
{
public:
	DbAccessor() = delete;
	~DbAccessor() = delete;
	DbAccessor(const DbAccessor&) = delete;
	DbAccessor(DbAccessor&) = delete;
	DbAccessor& operator=(const DbAccessor&) = delete;
	DbAccessor& operator=(DbAccessor&&) = delete;

private:
	static bool InitDb(){
		for (const auto& table:DataBase::db_table_list)
		{
			try
			{
				drogon::app().getDbClient()->execSqlSync(table);
			}
			catch (const std::exception& e)
			{
				LOG_ERROR << "can not create table: "<<table<<" , exception " << e.what();
				return false;
			}
			catch (const drogon::orm::DrogonDbException& e)
			{
				LOG_ERROR << "can not create table: " << table << " , database exception: " << e.base().what();
				return false;
			}
		}
		return true;
    }

public:
	static drogon::orm::DbClientPtr GetDbClient(){
        static std::once_flag flag;
	    std::call_once(flag, []
		{
			InitDb();
			LOG_INFO << "Database init success";
		});
	    return drogon::app().getDbClient();
    }

};