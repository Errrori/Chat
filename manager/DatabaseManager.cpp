#include "pch.h"
#include "../models/ChatRecords.h"
#include "../models/Users.h"
#include "../models/Relationships.h"
#include "DatabaseManager.h"
#include <drogon/orm/Mapper.h>

#include "models/Notifications.h"

using namespace DataBase;
using ChatRecord = drogon_model::sqlite3::ChatRecords;
using User = drogon_model::sqlite3::Users;
using Relationships = drogon_model::sqlite3::Relationships;
using Notification = drogon_model::sqlite3::Notifications;
using namespace drogon::orm;

void DatabaseManager::InitDatabase()
{
    drogon::app().getDbClient()->execSqlSync(USER_TABLE);
    drogon::app().getDbClient()->execSqlSync(CHAT_RECORDS_TABLE);
    drogon::app().getDbClient()->execSqlSync(RELATIONSHIPS_TABLE);
    drogon::app().getDbClient()->execSqlSync(GROUPS_TABLE);
    drogon::app().getDbClient()->execSqlSync(GROUP_MEMBERS_TABLE);
    drogon::app().getDbClient()->execSqlSync(NOTIFICATION_TABLE);
    drogon::app().getDbClient()->execSqlSync(CREATE_INDEX_1);
    drogon::app().getDbClient()->execSqlSync(CREATE_INDEX_2);
    drogon::app().getDbClient()->execSqlSync(CREATE_INDEX_3);
    drogon::app().getDbClient()->execSqlSync(CREATE_INDEX_4);
    drogon::app().getDbClient()->execSqlSync(CREATE_INDEX_5);
    drogon::app().getDbClient()->execSqlSync(CREATE_INDEX_6);
    drogon::app().getDbClient()->execSqlSync(CREATE_TRIGGER);
}

DbClientPtr DatabaseManager::GetDbClient()
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
	Mapper<User> mapper(GetDbClient());
	auto user = std::make_shared<User>(user_info);
	mapper.insert(*user,
		[](const User& info)
		{
			LOG_INFO << "Insert success,message username:" << info.getValueOfUsername();
		},[](const DrogonDbException& e)
		{
			LOG_ERROR << "exception to push user: " << e.base().what();
		});
	return true;

}
Json::Value DatabaseManager::GetAllUsersInfo()
{
	Mapper<User> mapper(GetDbClient());
	auto users = mapper.orderBy(User::Cols::_id, SortOrder::ASC).limit(100).findAll();
	Json::Value users_data(Json::arrayValue);
	for (const auto& user:users)
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

void DatabaseManager::PushChatRecords(const Json::Value& message)
{
	//如果想要保持数据库中插入json数据的一致，要手动创建
	//auto sender_name = message["sender_name"].asString();
	//auto sender_uid = message["sender_uid"].asString();
	//auto msg = message["content"].asString();
	//auto msg_id = message["msg_id"].asInt64();
	//chat_record->setSenderName(sender_name);
	//chat_record->setSenderUid(sender_uid);
	//chat_record->setContent(msg);
	//chat_record->setMessageId(msg_id);
	auto chat_record = std::make_shared<ChatRecord>(message);

	Mapper<ChatRecord> mapper(GetDbClient());

	mapper.insert(*chat_record,
		[](const ChatRecord& chat){
			LOG_INFO << "Insert success,message id:" << chat.getValueOfMessageId();
		},
		[](const DrogonDbException& e) {
			LOG_ERROR << "Exception to insert message," << e.base().what();
		}
	);
}

void DatabaseManager::WriteFriendRequest(const Json::Value& request)
{
	//可能要检查是否被屏蔽
	Json::Value data;
	data["first_uid"] = request["actor_uid"].asString();
	data["second_uid"] = request["reactor_uid"].asString();
	data["status"] = "pending";
	Relationships relation(data);

	Mapper<Relationships> mapper(GetDbClient());
	mapper.insert(relation,
		[](const Relationships& relationship) {
			LOG_INFO << "insert relationship: " << relationship.getValueOfStatus();
		},
		[](const DrogonDbException& e) {
			LOG_ERROR << "Exception to insert notification," << e.base().what();
		});
}

void DatabaseManager::PushNotification(const Json::Value& data)
{
	Json::Value json_notification;
	json_notification["notification_id"] = Utils::Authentication::GenerateUid();
	json_notification["actor_uid"] = data["actor_uid"].asString();
	json_notification["reactor_uid"] = data["reactor_uid"].asString();
	json_notification["action_type"] = data["action_type"].asString();
	json_notification["content"] = data["content"].asString();
	json_notification["create_time"] = data["create_time"].asString();
	Notification notification(json_notification);
	Mapper<Notification> mapper(GetDbClient());
	mapper.insert(notification,
		[](const Notification& notification) {
			LOG_INFO << "insert notification: " << notification.getValueOfNotificationId();
		},
		[](const DrogonDbException& e) {
			LOG_ERROR << "Exception to insert notification," << e.base().what();
		});
}

Json::Value DatabaseManager::GetChatRecords(int64_t existing_id, unsigned num)
{
	Mapper<ChatRecord> mapper(GetDbClient());

	//如果现有id不存在，直接返回
	auto exist_record =
		mapper.limit(1).findBy(Criteria(ChatRecord::Cols::_message_id, CompareOperator::EQ, existing_id));
	if (exist_record.empty())
	{
		LOG_ERROR << "can not find existing id:" << existing_id;
		return Json::nullValue;
	}

	Criteria criteria(ChatRecord::Cols::_message_id, CompareOperator::GT, existing_id);
	auto index_record = mapper.orderBy(ChatRecord::Cols::_message_id, SortOrder::ASC)
		.offset(num).limit(1).findBy(criteria);
	auto latest_record = mapper.orderBy(ChatRecord::Cols::_message_id, SortOrder::DESC).limit(1).findAll();
	auto latest_id = latest_record[0].getValueOfMessageId();
	Json::Value data(Json::arrayValue);
	//找不到，就从这个已有的位置开始向后返回到最新的
	if (index_record.empty())
	{
		const auto& records = mapper.orderBy(ChatRecord::Cols::_message_id,SortOrder::DESC).findBy(criteria);
		Json::Value json_records(Json::arrayValue);
		for (auto it = records.rbegin();it!=records.rend();++it)
		{
			json_records.append(WriteRecord(*it));
		}
		return json_records;
	}
	
	//找到了，就从最新的往前返回message_number条
	auto records = mapper.orderBy(ChatRecord::Cols::_message_id, SortOrder::DESC).limit(num).findAll();
	Json::Value json_records(Json::arrayValue);
	for (auto it = records.rbegin(); it != records.rend(); ++it)
	{
		json_records.append(WriteRecord(*it));
	}
	return json_records;

}

Json::Value DatabaseManager::GetAllRecords(unsigned num)
{
	Mapper<ChatRecord> mapper(GetDbClient());
	Json::Value data(Json::arrayValue);
	auto records =
		mapper.orderBy(ChatRecord::Cols::_message_id, SortOrder::DESC).limit(num).findAll();

	for (auto it = records.rbegin(); it != records.rend(); ++it)
	{
		data.append(WriteRecord(*it));
	}
	LOG_INFO << "Get all records:\n" << data.toStyledString()<<"\n";
	return data;
}

bool DatabaseManager::GetUserInfoByUid(const std::string& uid, Json::Value& data)
{
	Mapper<User> mapper(GetDbClient());
	Criteria criteria(User::Cols::_uid, CompareOperator::EQ, uid);
	auto info = mapper.limit(1).findBy(criteria);
	if (!info.empty())
	{
		data["username"] = info[0].getValueOfUsername();
		data["uid"] = info[0].getValueOfUid();
		data["create_time"] = info[0].getValueOfCreateTime();
		data["avatar"] = info[0].getValueOfAvatar();
		data["account"] = info[0].getValueOfAccount();
		return true;
	}
	LOG_ERROR << "can not find user info";
	return false;
}

bool DatabaseManager::GetUserInfoByAccount(const std::string& account,Json::Value& data)
{
	Mapper<User> mapper(GetDbClient());
	Criteria criteria(User::Cols::_account, CompareOperator::EQ, account);
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

bool DatabaseManager::GetUserNotification(const std::string& uid,Json::Value& data)
{
	Mapper<Notification> mapper(GetDbClient());
	Criteria criteria(Notification::Cols::_reactor_uid, uid);
	const auto& results = mapper.findBy(criteria);
	Json::Value notification_data(Json::arrayValue);
	for (const auto& result : results)
	{
		notification_data.append(WriteNotification(result));
	}
	data = notification_data;
	return true;
}

bool DatabaseManager::ModifyAvatar(const std::string& uid, const std::string& avatar)
{
	Mapper<User> mapper(GetDbClient());
	Criteria criteria(User::Cols::_uid,CompareOperator::EQ,uid);
	size_t result = mapper.limit(1).updateBy({User::Cols::_avatar},criteria,avatar);

	return result == 1;
}

bool DatabaseManager::ModifyUsername(const std::string& uid, const std::string& username)
{
	Mapper<User> mapper(GetDbClient());
	Criteria criteria(User::Cols::_uid, CompareOperator::EQ, uid);
	size_t result = mapper.updateBy({ User::Cols::_username }, criteria, username);

	return result == 1;
}

bool DatabaseManager::ModifyPassword(const std::string& uid, const std::string& password)
{
	Mapper<User> mapper(GetDbClient());
	Criteria criteria(User::Cols::_uid, CompareOperator::EQ, uid);
	size_t result = mapper.updateBy({ User::Cols::_password }, criteria, password);

	return result == 1;
}

bool DatabaseManager::DeleteUser(const std::string& uid)
{
	Mapper<User> mapper(GetDbClient());
	Criteria criteria(User::Cols::_uid, CompareOperator::EQ, uid);
	auto result = mapper.deleteBy(criteria);
	return result == 1;
}

bool DatabaseManager::ValidateAccount(const std::string& account)
{
	Mapper<User> mapper(GetDbClient());
	Criteria criteria(User::Cols::_account, CompareOperator::EQ, account);
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
	Mapper<User> mapper(GetDbClient());
	Criteria criteria(User::Cols::_uid, CompareOperator::EQ,uid);
	auto result = mapper.limit(1).findBy(criteria);
	return (!result.empty());
}

Json::Value DatabaseManager::WriteRecord(const drogon_model::sqlite3::ChatRecords& record)
{
	Json::Value json_record;
	json_record["message_id"] = std::to_string(record.getValueOfMessageId());
	json_record["sender_uid"] = record.getValueOfSenderUid();
	json_record["sender_name"] = record.getValueOfSenderName();
	json_record["avatar"] = record.getValueOfSenderAvatar();
	json_record["content"] = record.getValueOfContent();
	json_record["content_type"] = record.getValueOfContentType();
	json_record["receiver_uid"] = record.getValueOfReceiverUid();
	json_record["create_time"] = record.getValueOfCreateTime();
	json_record["conversation_id"] = record.getValueOfConversationId();
	json_record["conversation_name"] = record.getValueOfConversationName();
	json_record["chat_type"] = record.getValueOfChatType();

	return json_record;
}

Json::Value DatabaseManager::WriteNotification(const drogon_model::sqlite3::Notifications& notification)
{
	Json::Value data;
	data["actor_uid"] = notification.getValueOfActorUid();
	data["reactor_uid"] = notification.getValueOfReactorUid();
	data["action_type"] = notification.getValueOfActionType();
	data["create_time"] = notification.getValueOfCreateTime();
	data["content"] = notification.getValueOfContent();
	//data["status"] = notification.getValueOfStatus();
	//这里忽略了通知id和status
	return data;
}

