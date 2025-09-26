#include "pch.h"
#include "DatabaseManager.h"
#include "../models/Users.h"
#include <drogon/orm/Mapper.h>

#include "models/GroupChats.h"
#include "models/PrivateChats.h"
#include "models/Threads.h"
#include "models/GroupMembers.h"
#include "models/Messages.h"


using namespace DataBase;
using Users = drogon_model::sqlite3::Users;
using Messages = drogon_model::sqlite3::Messages;
using Threads = drogon_model::sqlite3::Threads;
using GroupChats = drogon_model::sqlite3::GroupChats;
using PrivateChats = drogon_model::sqlite3::PrivateChats;
using GroupMembers = drogon_model::sqlite3::GroupMembers;


using namespace drogon::orm;

void DatabaseManager::InitDatabase()
{
	drogon::app().getDbClient()->execSqlSync(USER_TABLE);
	drogon::app().getDbClient()->execSqlSync(THREAD_TABLE);
	drogon::app().getDbClient()->execSqlSync(PRIVATE_TABLE);
	drogon::app().getDbClient()->execSqlSync(GROUP_TABLE);
	drogon::app().getDbClient()->execSqlSync(AI_TABLE);
	drogon::app().getDbClient()->execSqlSync(GROUP_MEMBER_TABLE);
	drogon::app().getDbClient()->execSqlSync(MESSAGE_TABLE);

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

bool DatabaseManager::PushUser(const Json::Value& user_info)
{
	Mapper<Users> mapper(GetDbClient());
	auto user = std::make_shared<Users>(user_info);
	mapper.insert(*user,
		[](const Users& info)
		{
			LOG_INFO << "Insert success,message username:" << info.getValueOfUsername();
		}, [](const DrogonDbException& e)
			{
				LOG_ERROR << "exception to push user: " << e.base().what();
			});
		return true;
}

Json::Value DatabaseManager::GetAllUsersInfo()
{
	Mapper<Users> mapper(GetDbClient());
	auto users = mapper.orderBy(Users::Cols::_id, SortOrder::ASC).limit(100).findAll();
	Json::Value users_data(Json::arrayValue);
	for (const auto& user : users)
	{
		Json::Value data;
		data["username"] = user.getValueOfUsername();
		data["uid"] = user.getValueOfUid();
		data["avatar"] = user.getValueOfAvatar();
		data["create_time"] = user.getValueOfCreateTime();
		data["account"] = user.getValueOfAccount();
		users_data.append(data);
	}
	return users_data;
}


bool DatabaseManager::GetUserInfoByUid(const std::string& uid, Json::Value& data)
{
	Mapper<Users> mapper(GetDbClient());
	Criteria criteria(Users::Cols::_uid, CompareOperator::EQ, uid);
	auto info = mapper.limit(1).findBy(criteria);
	if (!info.empty())
	{
		data["username"] = info[0].getValueOfUsername();
		data["uid"] = info[0].getValueOfUid();
		data["create_time"] = info[0].getValueOfCreateTime();
		data["avatar"] = info[0].getValueOfAvatar();
		data["account"] = info[0].getValueOfAccount();
		data["status"] = Json::Value::Int64(info[0].getValueOfStatus());
		data["level"] = Json::Value::Int64(info[0].getValueOfLevel());
		data["posts"] = Json::Value::Int64(info[0].getValueOfPosts());
		data["followers"] = Json::Value::Int64(info[0].getValueOfFollowers());
		data["following"] = Json::Value::Int64(info[0].getValueOfFollowing());
		data["last_login_time"] = info[0].getValueOfLastLoginTime();
		data["signature"] = info[0].getValueOfSignature();
		data["email"] = info[0].getValueOfEmail();
		return true;
	}
	LOG_ERROR << "can not find user info";
	return false;
}

bool DatabaseManager::GetUserInfoByAccount(const std::string& account, Json::Value& data)
{
	Mapper<Users> mapper(GetDbClient());
	Criteria criteria(Users::Cols::_account, CompareOperator::EQ, account);
	auto info = mapper.limit(1).findBy(criteria);
	if (!info.empty())
	{
		data["username"] = info[0].getValueOfUsername();
		data["uid"] = info[0].getValueOfUid();
		data["account"] = info[0].getValueOfAccount();
		data["password"] = info[0].getValueOfPassword();
		return true;
	}
	return false;
}

bool DatabaseManager::GetUserQueryInfoByAccount(const std::string& account, Json::Value& data)
{
	Mapper<Users> mapper(GetDbClient());
	Criteria criteria(Users::Cols::_account, CompareOperator::EQ, account);
	auto info = mapper.limit(1).findBy(criteria);
	if (!info.empty())
	{
		data["username"] = info[0].getValueOfUsername();
		data["uid"] = info[0].getValueOfUid();
		data["create_time"] = info[0].getValueOfCreateTime();
		data["avatar"] = info[0].getValueOfAvatar();
		data["account"] = info[0].getValueOfAccount();
		data["status"] = Json::Value::Int64(info[0].getValueOfStatus());
		data["level"] = Json::Value::Int64(info[0].getValueOfLevel());
		data["posts"] = Json::Value::Int64(info[0].getValueOfPosts());
		data["followers"] = Json::Value::Int64(info[0].getValueOfFollowers());
		data["following"] = Json::Value::Int64(info[0].getValueOfFollowing());
		data["last_login_time"] = info[0].getValueOfLastLoginTime();
		data["signature"] = info[0].getValueOfSignature();
		data["email"] = info[0].getValueOfEmail();
		return true;
	}
	LOG_ERROR << "can not find user info";
	return false;
}

bool DatabaseManager::ModifyAvatar(const std::string& uid, const std::string& avatar)
{
	Mapper<Users> mapper(GetDbClient());
	Criteria criteria(Users::Cols::_uid, CompareOperator::EQ, uid);
	size_t result = mapper.limit(1).updateBy({ Users::Cols::_avatar }, criteria, avatar);

	return result == 1;
}

bool DatabaseManager::ModifyUsername(const std::string& uid, const std::string& username)
{
	Mapper<Users> mapper(GetDbClient());
	Criteria criteria(Users::Cols::_uid, CompareOperator::EQ, uid);
	size_t result = mapper.updateBy({ Users::Cols::_username }, criteria, username);

	return result == 1;
}

bool DatabaseManager::ModifyPassword(const std::string& uid, const std::string& password)
{
	Mapper<Users> mapper(GetDbClient());
	Criteria criteria(Users::Cols::_uid, CompareOperator::EQ, uid);
	size_t result = mapper.updateBy({ Users::Cols::_password }, criteria, password);

	return result == 1;
}

bool DatabaseManager::ModifyUserInfo(const Utils::UsersInfo& info)
{
	Mapper<Users> mapper(GetDbClient());
	Criteria criteria;
	if (!info.uid.empty())
	{
		criteria = Criteria(Users::Cols::_uid, CompareOperator::EQ, info.uid);
	}
	else if (!info.account.empty())
	{
		criteria = Criteria(Users::Cols::_account, CompareOperator::EQ, info.account);
	}
	size_t result = mapper.updateBy({ Users::Cols::_avatar,Users::Cols::_username,Users::Cols::_email,Users::Cols::_signature }, criteria,
		info.avatar,info.username,info.email,info.signature);
	return result>0;
}

bool DatabaseManager::DeleteUser(const std::string& uid)
{
	Mapper<Users> mapper(GetDbClient());
	Criteria criteria(Users::Cols::_uid, CompareOperator::EQ, uid);
	auto result = mapper.deleteBy(criteria);
	return result == 1;
}

//这里是验证已经注册的账号是否合法
bool DatabaseManager::ValidateAccount(const std::string& account)
{
	Mapper<Users> mapper(GetDbClient());
	Criteria criteria(Users::Cols::_account, CompareOperator::EQ, account);
	auto user = mapper.limit(1).findBy(criteria);
	if (user.empty())
	{
		return true;
	}
	LOG_ERROR << "user is existed";
	return false;
}

bool DatabaseManager::ValidateUid(const std::string& uid)
{
	Mapper<Users> mapper(GetDbClient());
	Criteria criteria(Users::Cols::_uid, CompareOperator::EQ, uid);
	auto result = mapper.limit(1).findBy(criteria);
	return (!result.empty());
}

bool DatabaseManager::ValidateThreadId(unsigned thread_id)
{
	Mapper<Threads> mapper(GetDbClient());
	Criteria criteria(Threads::Cols::_thread_id, CompareOperator::EQ, thread_id);
	auto result = mapper.limit(1).findBy(criteria);
	return (!result.empty());
}


Json::Value DatabaseManager::GetMessages(unsigned thread_id, int64_t existing_id, unsigned num)
{
	Mapper<Messages> mapper(GetDbClient());

	// 如果现有id不存在，直接返回
	const auto& exist_record =
		mapper.limit(1).findBy
	(Criteria(Criteria(Messages::Cols::_message_id, CompareOperator::EQ, existing_id)
		&& Criteria(Messages::Cols::_thread_id, CompareOperator::EQ, thread_id)));
	if (exist_record.empty())
	{
		LOG_ERROR << "can not find existing message id:" << existing_id;
		return Json::nullValue;
	}

	Criteria criteria(Criteria(Messages::Cols::_message_id, CompareOperator::GT, existing_id)
		&& Criteria(Messages::Cols::_thread_id, CompareOperator::EQ, thread_id));
	const auto& index_record = mapper.orderBy(Messages::Cols::_message_id, SortOrder::ASC)
		.offset(num).limit(1).findBy(criteria);
	const auto& latest_record = mapper.orderBy(Messages::Cols::_message_id, SortOrder::DESC).limit(1).findAll();
	auto latest_id = latest_record[0].getValueOfMessageId();
	Json::Value data(Json::arrayValue);

	// 找不到，就从这个已有的位置开始向后返回到最新的
	if (index_record.empty())
	{
		const auto& records = mapper.orderBy(Messages::Cols::_message_id, SortOrder::DESC).findBy(criteria);
		Json::Value json_records(Json::arrayValue);
		for (auto it = records.rbegin(); it != records.rend(); ++it)
		{
			json_records.append(it->toJson());
		}
		return json_records;
	}

	const auto& records = mapper.orderBy(Messages::Cols::_message_id, SortOrder::DESC).limit(num)
		.findBy(Criteria(Messages::Cols::_thread_id, CompareOperator::EQ, thread_id));
	Json::Value json_records(Json::arrayValue);
	for (auto it = records.rbegin(); it != records.rend(); ++it)
	{
		json_records.append(it->toJson());
	}
	return json_records;
}

Json::Value DatabaseManager::GetAllMessage(unsigned thread_id, unsigned num)
{
	Mapper<Messages> mapper(GetDbClient());
	Criteria criteria(Messages::Cols::_thread_id, CompareOperator::EQ, thread_id);
	Json::Value data(Json::arrayValue);
	auto records =
		mapper.orderBy(Messages::Cols::_message_id, SortOrder::DESC).limit(num).findBy(criteria);

	for (auto it = records.rbegin(); it != records.rend(); ++it)
	{
		data.append(it->toJson());
	}
	LOG_INFO << "Get all messages:\n" << data.toStyledString() << "\n";
	return data;
}
