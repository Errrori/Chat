#include "pch.h"
#include "../models/ChatRecords.h"
#include "../models/Users.h"
#include "../models/Relationships.h"
#include "DatabaseManager.h"
#include <drogon/orm/Mapper.h>

#include "DTOs/NotificationDTO.h"
#include "models/Notifications.h"
#include "DTOs/RelationshipDTO.h"
#include "models/GroupChats.h"
#include "models/PrivateChats.h"
#include "models/Threads.h"
#include "models/GroupMembers.h"
#include "models/Messages.h"


using namespace DataBase;
using ChatRecords = drogon_model::sqlite3::ChatRecords;
using Users = drogon_model::sqlite3::Users;
using Relationships = drogon_model::sqlite3::Relationships;
using Notifications = drogon_model::sqlite3::Notifications;
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
	drogon::app().getDbClient()->execSqlSync(GROUP_MEMBER_TABLE);
	drogon::app().getDbClient()->execSqlSync(MESSAGE_TABLE);
	CreatePublicThread();

	//drogon::app().getDbClient()->execSqlSync(RELATIONSHIPS_TABLE);
	//drogon::app().getDbClient()->execSqlSync(CHAT_RECORDS_TABLE);
   // drogon::app().getDbClient()->execSqlSync(GROUP_MEMBERS_TABLE);
	//drogon::app().getDbClient()->execSqlSync(NOTIFICATION_TABLE);
	//drogon::app().getDbClient()->execSqlSync(CREATE_INDEX_1);
	//drogon::app().getDbClient()->execSqlSync(CREATE_INDEX_2);
	//drogon::app().getDbClient()->execSqlSync(CREATE_INDEX_3);
	//drogon::app().getDbClient()->execSqlSync(CREATE_INDEX_4);
	//drogon::app().getDbClient()->execSqlSync(CREATE_INDEX_5);
	//drogon::app().getDbClient()->execSqlSync(CREATE_INDEX_6);
	//drogon::app().getDbClient()->execSqlSync(CREATE_TRIGGER);

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

void DatabaseManager::PushChatRecords(const drogon_model::sqlite3::ChatRecords& chat_record)
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

	Mapper<ChatRecords> mapper(GetDbClient());

	mapper.insert(chat_record,
		[](const ChatRecords& chat) {
			LOG_INFO << "Insert success,message id:" << chat.getValueOfMessageId();
		},
		[](const DrogonDbException& e) {
			LOG_ERROR << "Exception to insert message," << e.base().what();
		}
	);
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
	Notifications notification(json_notification);
	Mapper<Notifications> mapper(GetDbClient());
	mapper.insert(notification,
		[](const Notifications& notification) {
			LOG_INFO << "insert notification: " << notification.getValueOfNotificationId();
		},
		[](const DrogonDbException& e) {
			LOG_ERROR << "Exception to insert notification," << e.base().what();
		});
}

bool DatabaseManager::WriteRelationship(const RelationshipDTO& dto, std::string& error_msg)
{
	using Type = Utils::UserAction::RelationAction::RelationshipActionType;
	using Status = Utils::Relationship::StatusType;
	const static std::unordered_map<Type,
		std::function<bool(const RelationshipDTO&, std::string&)>>
		action_map = {
		{ Type::UnblockUser, [](const RelationshipDTO& dto, std::string& error_msg) {
			return DeleteRelationship(dto.GetActorUid(), dto.GetReactorUid(), Status::Blocking, error_msg);
		} },
		{ Type::RequestReject, [](const RelationshipDTO& dto, std::string& error_msg) {
			return DeleteRelationship(dto.GetReactorUid(), dto.GetActorUid(), Status::Pending, error_msg);
		} },
		{ Type::Unfriend, [](const RelationshipDTO& dto, std::string& error_msg) {
			return DeleteRelationship(dto.GetActorUid(), dto.GetReactorUid(), Status::Friend, error_msg);
		} },
		{ Type::BlockUser, [](const RelationshipDTO& dto, std::string& error_msg) {
			return UpsertRelationship(dto.GetActorUid(), dto.GetReactorUid(),Status::Blocking,error_msg);
		} },
		{ Type::FriendRequest, [](const RelationshipDTO& dto, std::string& error_msg) {
			return UpsertRelationship(dto.GetActorUid(), dto.GetReactorUid(),Status::Pending,error_msg);
		} },
		{ Type::RequestAccept, [](const RelationshipDTO& dto, std::string& error_msg) {
			return UpsertRelationship(dto.GetReactorUid(), dto.GetActorUid(),Status::Friend,error_msg);
		} }
	};

	const auto action_type = dto.GetActionType();
	auto it = action_map.find(action_type);
	if (it != action_map.end())
	{
		return it->second(dto, error_msg);
	}
	error_msg = "unsupported action type";
	return false;
}

//bool DatabaseManager::WriteRelationship(const RelationshipDTO& dto, std::string& error_msg)
//{
//	Mapper<Relationships> mapper(GetDbClient());
//	using Type = Utils::UserAction::RelationAction::RelationshipActionType;
//	switch (dto.GetActionType())
//	{
//	case Type::Unknown:
//		LOG_ERROR << "Unsupported type";
//		return false;
//	//这部分是删除操作
//	case Type::UnblockUser:
//		return DeleteRelationship(dto.GetActorUid(), dto.GetReactorUid(), "Blocking");
//	case Type::RequestReject:
//		return DeleteRelationship(dto.GetReactorUid(), dto.GetActorUid(), "Pending");
//	case Type::Unfriend:
//		return DeleteRelationship(dto.GetActorUid(), dto.GetReactorUid(), "Friend");
//	//这部分是更新和插入操作
//	case Type::BlockUser:
//	case Type::FriendRequest:
//	case Type::RequestAccept:
//		return UpsertRelationship(dto);
//	}
//	return false;
//}

bool DatabaseManager::UpsertRelationship
(const std::string& first_uid, const std::string& second_uid, Utils::Relationship::StatusType status, std::string& error_msg)
{
	if (first_uid == second_uid)
	{
		error_msg = "two uid are the same";
		return false;
	}

	Mapper<Relationships> mapper(GetDbClient());
	using Type = Utils::Relationship::StatusType;

	std::promise<bool> promise;
	std::future<bool> result = promise.get_future();

	switch (status)
	{
	case Type::Unknown:
		error_msg = "can not upsert unknown type";
		return false;
	case Type::Pending:
	case Type::Blocking:
	{
		Relationships relationship;
		relationship.setFirstUid(first_uid);
		relationship.setSecondUid(second_uid);
		relationship.setStatus(Utils::Relationship::TypeToString(status));
		try
		{
			const auto& future = mapper.insertFuture(relationship).get();
			promise.set_value(true);
		}
		catch (const std::exception& e)
		{
			promise.set_value(false);
			LOG_ERROR << "exception insert relationship: " << e.what();
		}

		//mapper.insert(relationship,
		//	[&promise](const Relationships& relationship)
		//	{
		//		promise.set_value(true);
		//		LOG_INFO << "success insert a relationship: " << relationship.getValueOfStatus();
		//	},
		//	[&error_msg](const DrogonDbException& e)
		//	{
		//		LOG_ERROR<<"exception: "<<e.base().what();
		//		error_msg = "Exception insert";
		//		error_msg.append(e.base().what());
		//	});
		break;
	}
	case Type::Friend:
	{
		std::string normalized_first_uid = first_uid;
		std::string	normalized_second_uid = second_uid;
		//因为uid的长度是一致的，所以采用字典序比较
		if (normalized_first_uid > normalized_second_uid)
		{
			std::swap(normalized_first_uid, normalized_second_uid);
		}
		Criteria criteria(Criteria(Relationships::Cols::_first_uid, CompareOperator::EQ, first_uid)
			&& Criteria(Relationships::Cols::_second_uid, CompareOperator::EQ, second_uid));
		using Cols = Relationships::Cols;
		bool is_success = mapper.limit(1).findBy(criteria).empty();

		if (is_success)
		{
			LOG_ERROR << "can not find record";
			return false;
		}

		mapper.limit(1).updateBy({ Cols::_first_uid,Cols::_second_uid,Cols::_status },
			[&promise](const size_t)
			{
				promise.set_value(true);
			},
			[&error_msg](const DrogonDbException& e)
			{
				error_msg = "exception to update";
				error_msg.append(e.base().what());
				LOG_ERROR << "exception to update: " << e.base().what();
			},
			criteria, normalized_first_uid, normalized_second_uid, Utils::Relationship::TypeToString(status));
		break;
	}
	}
	return result.get();
}

bool DatabaseManager::WriteNotification(const NotificationDTO& dto)
{
	const auto& notification = dto.ToNotifications();
	if (!notification.has_value())
	{
		return false;
	}
	Mapper<Notifications> mapper(GetDbClient());
	bool is_success = false;
	mapper.insert(notification.value(),
		[&is_success](const Notifications& data)
		{
			is_success = true;
			LOG_INFO << "Insert a notification: " + data.getValueOfNotificationId();
		},
		[](const DrogonDbException& e)
		{
			LOG_ERROR << "Exception to insert notification: " << e.base().what();
		});
	return is_success;
}

bool DatabaseManager::DeleteRelationship
(const std::string& first_uid, const std::string& second_uid, Utils::Relationship::StatusType status, std::string& error_msg)
{
	if (status == Utils::Relationship::StatusType::Unknown) return false;
	Mapper<Relationships> mapper(GetDbClient());
	using Type = Utils::Relationship::StatusType;
	Criteria criteria;
	switch (status) {
	case Type::Friend:
	{
		std::string normalized_first_uid = first_uid;
		std::string normalized_second_uid = second_uid;
		//因为uid的长度是一致的，所以采用字典序比较
		if (normalized_first_uid > normalized_second_uid)
		{
			std::swap(normalized_first_uid, normalized_second_uid);
		}
		criteria = Criteria(Criteria(Relationships::Cols::_first_uid, CompareOperator::EQ, normalized_first_uid)
			&& Criteria(Relationships::Cols::_second_uid, CompareOperator::EQ, normalized_second_uid)
			&& Criteria(Relationships::Cols::_status, CompareOperator::EQ, status));
		break;
	}
	case Type::Blocking:
	case Type::Pending:
		criteria = Criteria(Criteria(Relationships::Cols::_first_uid, CompareOperator::EQ, first_uid)
			&& Criteria(Relationships::Cols::_second_uid, CompareOperator::EQ, second_uid)
			&& Criteria(Relationships::Cols::_status, CompareOperator::EQ, status));
		break;
	default:
		LOG_ERROR << "can not delete unknown record";
		error_msg = "can not delete Unknown type record";
		return false;
	}
	bool is_success = false;
	mapper.limit(1).deleteBy(criteria,
		[&is_success](const size_t size) {
			is_success = true;
			LOG_INFO << "delete relationship number: " << size;
		},
		[&error_msg](const DrogonDbException& e) {
			LOG_ERROR << "Exception to delete relationship , " << e.base().what();
			error_msg = "exception to delete: ";
			error_msg.append(e.base().what());
		});
	return is_success;
}

Json::Value DatabaseManager::GetChatRecords(int64_t existing_id, unsigned num)
{
	Mapper<ChatRecords> mapper(GetDbClient());

	//如果现有id不存在，直接返回
	auto exist_record =
		mapper.limit(1).findBy(Criteria(ChatRecords::Cols::_message_id, CompareOperator::EQ, existing_id));
	if (exist_record.empty())
	{
		LOG_ERROR << "can not find existing id:" << existing_id;
		return Json::nullValue;
	}

	Criteria criteria(ChatRecords::Cols::_message_id, CompareOperator::GT, existing_id);
	auto index_record = mapper.orderBy(ChatRecords::Cols::_message_id, SortOrder::ASC)
		.offset(num).limit(1).findBy(criteria);
	auto latest_record = mapper.orderBy(ChatRecords::Cols::_message_id, SortOrder::DESC).limit(1).findAll();
	auto latest_id = latest_record[0].getValueOfMessageId();
	Json::Value data(Json::arrayValue);
	//找不到，就从这个已有的位置开始向后返回到最新的
	if (index_record.empty())
	{
		const auto& records = mapper.orderBy(ChatRecords::Cols::_message_id, SortOrder::DESC).findBy(criteria);
		Json::Value json_records(Json::arrayValue);
		for (auto it = records.rbegin(); it != records.rend(); ++it)
		{
			json_records.append(it->toJson());
		}
		return json_records;
	}
	auto records = mapper.orderBy(ChatRecords::Cols::_message_id, SortOrder::DESC).limit(num).findAll();
	Json::Value json_records(Json::arrayValue);
	for (auto it = records.rbegin(); it != records.rend(); ++it)
	{
		json_records.append(it->toJson());
	}
	return json_records;

}

Json::Value DatabaseManager::GetAllRecords(unsigned num)
{
	Mapper<ChatRecords> mapper(GetDbClient());
	Json::Value data(Json::arrayValue);
	auto records =
		mapper.orderBy(ChatRecords::Cols::_message_id, SortOrder::DESC).limit(num).findAll();

	for (auto it = records.rbegin(); it != records.rend(); ++it)
	{
		data.append(it->toJson());
	}
	LOG_INFO << "Get all records:\n" << data.toStyledString() << "\n";
	return data;
}

Json::Value DatabaseManager::GetRelationships()
{
	Mapper<Relationships> mapper(GetDbClient());
	Json::Value data(Json::arrayValue);
	const auto& relationships = mapper.findAll();

	for (const auto& relationship : relationships)
	{
		data.append(relationship.toJson());
	}

	return data;
}

Json::Value DatabaseManager::GetNotifications()
{
	Mapper<Notifications> mapper(GetDbClient());
	Json::Value data(Json::arrayValue);
	const auto& notices = mapper.findAll();

	for (const auto& notice : notices)
	{
		data.append(notice.toJson());
	}

	return data;
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

bool DatabaseManager::GetUserNotification(const std::string& uid, Json::Value& data)
{
	Mapper<Notifications> mapper(GetDbClient());
	Criteria criteria(Notifications::Cols::_receiver_uid, uid);
	const auto& results = mapper.findBy(criteria);
	Json::Value notification_data(Json::arrayValue);
	for (const auto& result : results)
	{
		notification_data.append(result.toJson());
	}
	data = notification_data;
	return true;
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

bool DatabaseManager::ValidateRelationship(const std::string& actor_uid, const std::string& reactor_uid,
	Utils::Relationship::StatusType status)
{
	using Type = Utils::Relationship::StatusType;
	Mapper<Relationships> mapper(GetDbClient());
	switch (status)
	{
	case Type::Pending:
	case Type::Blocking:
	{
		Criteria criteria(Criteria(Relationships::Cols::_first_uid, CompareOperator::EQ, actor_uid)
			&& Criteria(Relationships::Cols::_second_uid, CompareOperator::EQ, reactor_uid)
			&& Criteria(Relationships::Cols::_status, CompareOperator::EQ, status));
		return !mapper.findBy(criteria).empty();
	}
	case Type::Friend:
	{
		std::string normalized_first_uid = actor_uid;
		std::string	normalized_second_uid = reactor_uid;
		//因为uid的长度是一致的，所以采用字典序比较
		if (normalized_first_uid > normalized_second_uid)
		{
			std::swap(normalized_first_uid, normalized_second_uid);
		}
		Criteria criteria(Criteria(Relationships::Cols::_first_uid, CompareOperator::EQ, normalized_first_uid)
			&& Criteria(Relationships::Cols::_second_uid, CompareOperator::EQ, normalized_second_uid)
			&& Criteria(Relationships::Cols::_status, CompareOperator::EQ, status));
		return !mapper.findBy(criteria).empty();
	}
	default:
		LOG_ERROR << "try to validate unknown type";
		return false;
	}
}

bool DatabaseManager::PushMessage(const TransMsg& msg)
{
	Messages message;
	message.setMessageId(msg.message_id);
	message.setThreadId(msg.thread_id);
	message.setSenderUid(msg.sender_uid);
	message.setSenderName(msg.sender_name);
	message.setCreateTime(msg.create_time);
	message.setUpdateTime(msg.update_time);
	message.setSenderAvatar(msg.sender_avatar);
	message.setContent(msg.content.value_or(""));
	message.setAttachment(msg.attachment.value_or(""));

	Mapper<Messages> mapper(GetDbClient());
	mapper.insert(message,
		[](const Messages& data)
		{
			LOG_INFO << "Insert new message successful";
		},
		[](const DrogonDbException& e)
		{
			LOG_ERROR << "Exception to insert message: " << e.base().what();
		});
	return true;
}

Json::Value DatabaseManager::GetMessages(int64_t existing_id, unsigned num)
{
	Mapper<Messages> mapper(GetDbClient());

	// 如果现有id不存在，直接返回
	auto exist_record =
		mapper.limit(1).findBy(Criteria(Messages::Cols::_message_id, CompareOperator::EQ, existing_id));
	if (exist_record.empty())
	{
		LOG_ERROR << "can not find existing message id:" << existing_id;
		return Json::nullValue;
	}

	Criteria criteria(Messages::Cols::_message_id, CompareOperator::GT, existing_id);
	auto index_record = mapper.orderBy(Messages::Cols::_message_id, SortOrder::ASC)
		.offset(num).limit(1).findBy(criteria);
	auto latest_record = mapper.orderBy(Messages::Cols::_message_id, SortOrder::DESC).limit(1).findAll();
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
	auto records = mapper.orderBy(Messages::Cols::_message_id, SortOrder::DESC).limit(num).findAll();
	Json::Value json_records(Json::arrayValue);
	for (auto it = records.rbegin(); it != records.rend(); ++it)
	{
		json_records.append(it->toJson());
	}
	return json_records;
}

Json::Value DatabaseManager::GetAllMessage(unsigned num)
{
	Mapper<Messages> mapper(GetDbClient());
	Json::Value data(Json::arrayValue);
	auto records =
		mapper.orderBy(Messages::Cols::_message_id, SortOrder::DESC).limit(num).findAll();

	for (auto it = records.rbegin(); it != records.rend(); ++it)
	{
		data.append(it->toJson());
	}
	LOG_INFO << "Get all messages:\n" << data.toStyledString() << "\n";
	return data;
}

void DatabaseManager::CreatePublicThread()
{
	Mapper<Threads> mapper(drogon::app().getDbClient());
	Threads thread;
	thread.setId(PUBLIC_CHAT_ID);
	thread.setType(0);
	thread.setCreateTime(trantor::Date::now().toDbString());
	mapper.insert(thread,
		[](const Threads& t) {
			LOG_INFO << "Public thread created successfully";
		},
		[](const DrogonDbException& e) {
			LOG_ERROR << "Failed to create public thread: " << e.base().what();
		});
}

