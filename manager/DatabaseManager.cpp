#include "pch.h"
#include "../models/ChatRecords.h"
#include "../models/Users.h"
#include "../models/Relationships.h"
#include "DatabaseManager.h"
#include <drogon/orm/Mapper.h>

#include "DTOs/NotificationDTO.h"
#include "models/Notifications.h"
#include "DTOs/RelationshipDTO.h"

using namespace DataBase;
using ChatRecord = drogon_model::sqlite3::ChatRecords;
using Users = drogon_model::sqlite3::Users;
using Relationships = drogon_model::sqlite3::Relationships;
using Notifications = drogon_model::sqlite3::Notifications;
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
	Mapper<Users> mapper(GetDbClient());
	auto user = std::make_shared<Users>(user_info);
	mapper.insert(*user,
		[](const Users& info)
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
	Mapper<Users> mapper(GetDbClient());
	auto users = mapper.orderBy(Users::Cols::_id, SortOrder::ASC).limit(100).findAll();
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

bool DatabaseManager::WriteRelationship(const RelationshipDTO& dto,std::string& error_msg)
{
	using Type = Utils::UserAction::RelationAction::RelationshipActionType;
	using Status = Utils::Relationship::StatusType;
	const static std::unordered_map<Type,
		std::function<bool(const RelationshipDTO&, std::string&)>>
		action_map = {
		{ Type::UnblockUser, [](const RelationshipDTO& dto, std::string& error_msg) {
			return DeleteRelationship(dto.GetActorUid(), dto.GetReactorUid(), Status::Blocking, error_msg);
		} },
		{ Type::RequestReject, [](const RelationshipDTO & dto, std::string & error_msg) {
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
	if (it!=action_map.end())
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
	(const std::string& first_uid, const std::string& second_uid, Utils::Relationship::StatusType status,std::string& error_msg)
{
	if (first_uid == second_uid)
	{
		error_msg = "two uid are the same";
		return false;
	}

	Mapper<Relationships> mapper(GetDbClient());
	using Type = Utils::Relationship::StatusType;
	bool is_success = false;
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
		mapper.insert(relationship,
			[&is_success](const Relationships& relationship)
			{
				is_success = true;
				LOG_INFO << "success insert a relationship: " << relationship.getValueOfStatus();
			},
			[&error_msg](const DrogonDbException& e)
			{
				LOG_ERROR<<"exception: "<<e.base().what();
				error_msg = "Exception insert";
				error_msg.append(e.base().what());
			});
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
		mapper.limit(1).updateBy({ Cols::_first_uid,Cols::_second_uid,Cols::_status },
			[&is_success](const size_t)
			{
				is_success = true;
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
	return is_success;
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
			LOG_INFO << "delete relationship number: "<<size;
		},
		[&error_msg](const DrogonDbException& e) {
			LOG_ERROR << "Exception to delete relationship , " << e.base().what();
			error_msg = "exception to delete: ";
			error_msg.append(e.base().what());
	});
	return is_success;
}

//bool DatabaseManager::WriteRelationship(const RelationshipDTO& dto, std::string& error_msg)
//{
//	using Type = Utils::UserAction::RelationshipActionType;
//	std::string status;
//	bool is_update = false;
//	//检查操作合法性
//	switch (Utils::UserAction::StringToType(dto.GetActionType()))
//	{
//	case Type::Unknown:
//		error_msg = "unknown type";
//		return false;
//	case Type::FriendRequest:
//		//申请好友,如果屏蔽对方/被屏蔽的话要先解除屏蔽才能申请,如果对方给你发送过好友请求，你需要处理完请求之后才能发送请求
//		if (ValidateRelationship(dto.GetActorUid(), dto.GetReactorUid(),"Pending"))
//		{
//			error_msg = "you already sent a request";
//			return false;
//		}
//		if (ValidateRelationship(dto.GetReactorUid(), dto.GetActorUid(), "Pending"))
//		{
//			error_msg = "you need to handle the request from target user";
//			return false;
//		}
//		if (ValidateRelationship(dto.GetActorUid(), dto.GetReactorUid(), "Blocking"))
//		{
//			error_msg = "you need to unblock the user";
//			return false;
//		}
//		if (ValidateRelationship(dto.GetActorUid(), dto.GetReactorUid(), "Friend"))
//		{
//			error_msg = "you are already friends";
//			return false;
//		}
//		status = "Pending";
//		break;
//	case Type::RequestReject:
//		//拒绝好友申请的话，对方必须要给当前用户发送好友申请
//		if (!ValidateRelationship(dto.GetReactorUid(), dto.GetActorUid(), "Pending"))
//		{
//			error_msg = "target user did not send request";
//			return false;
//		}
//		status = "Rejected";
//		is_update = true;
//		break;
//	case Type::RequestAccept:
//		if (!ValidateRelationship(dto.GetReactorUid(), dto.GetActorUid(), "Pending"))
//		{
//			error_msg = "target user did not send request";
//			return false;
//		}
//		status = "Friend";
//		is_update = true;
//		break;
//	case Type::BlockUser:
//		if (ValidateRelationship(dto.GetActorUid(), dto.GetReactorUid(), "Blocking"))
//		{
//			error_msg = "you had already blocked the user";
//			return false;
//		}
//		if (ValidateRelationship(dto.GetActorUid(), dto.GetReactorUid(), "Friend"))
//		{
//			status = "BlockFriend";
//			is_update = true;
//		}
//		else
//			status = "Blocking";
//		break;
//	case Type::UnblockUser:
//		//如果之前是朋友，就恢复朋友关系，不是就删除关系
//		if (ValidateRelationship(dto.GetActorUid(), dto.GetReactorUid(), "Blocking"))
//		{
//			DeleteRelationship(dto.GetActorUid(), dto.GetReactorUid(), "Blocking");
//			return true;
//		}
//		if (ValidateRelationship(dto.GetActorUid(), dto.GetReactorUid(), "BlockFriend"))
//		{
//			is_update = true;
//			status = "Friend";
//		}
//		else
//		{
//			error_msg = "you did not block the user";
//			return false;
//		}
//		break;
//	case Type::Unfriend:
//		if (ValidateRelationship(dto.GetActorUid(), dto.GetReactorUid(), "Friend"))
//		{
//			DeleteRelationship(dto.GetActorUid(), dto.GetReactorUid(), "Friend");
//			return true;
//		}
//		if (ValidateRelationship(dto.GetActorUid(), dto.GetReactorUid(), "BlockFriend"))
//		{
//			DeleteRelationship(dto.GetActorUid(), dto.GetReactorUid(), "BlockFriend");
//			return true;
//		}
//		else
//		{
//			error_msg = "you are not friend";
//			return false;
//		}
//	}
//
//	//UpdateOrInsertRelationship(dto.GetActorUid(), dto.GetReactorUid(), status, is_update);
//
//}

//bool DatabaseManager::WriteRelationship(const RelationshipDTO& dto, std::string& error_msg)
//{
//	//pending/refuse/friend/block/unblock(解除屏蔽就删除在关系表中原有的记录)
//	Mapper<Relationships> mapper(GetDbClient());
//
//	Criteria criteria(Criteria(Relationships::Cols::_first_uid, CompareOperator::EQ, dto.GetActorUid()) &&
//		Criteria(Relationships::Cols::_second_uid, CompareOperator::EQ, dto.GetReactorUid()));
//
//	using RelationshipActionType = Utils::UserAction::RelationshipActionType;
//	switch (Utils::UserAction::StringToType(dto.GetActionType()))
//	{
//	case RelationshipActionType::BlockUser:
//		if (ValidateIsBlock(dto.GetActorUid(), dto.GetReactorUid()))
//		{
//			LOG_INFO << "block user repeatedly";
//			return true;
//		}
//		if (ValidateIsFriend(dto.GetActorUid(),dto.GetReactorUid()))
//		{
//			auto result = mapper.updateBy({ Relationships::Cols::_status }, criteria, "BlockFriend");
//			return result == 1;
//		}
//		else
//		{
//			auto data = dto.ToRelationshipJson();
//			data["status"] = "Blocking";
//			Relationships relationship(data);
//			bool is_success = false;
//			mapper.insert(relationship,
//				[&is_success](const Relationships& relationship) {
//					is_success = true;
//					LOG_INFO << "insert relationship: " << relationship.getValueOfStatus();
//				},
//				[](const DrogonDbException& e) {
//					LOG_ERROR << "Exception to insert notification," << e.base().what();
//				});
//			return is_success;
//		}
//	case RelationshipActionType::UnblockUser:
//		if (!ValidateIsBlock(dto.GetActorUid(), dto.GetReactorUid()))
//		{
//			LOG_ERROR << "can not unblock user that is not blocked";
//			return false;
//		}
//		if (ValidateIsFriend(dto.GetActorUid(), dto.GetReactorUid()))
//		{
//			auto result = mapper.updateBy({ Relationships::Cols::_status }, criteria, "Friend");
//			return result == 1;
//		}
//		else//解除屏蔽，但是两个用户此前没有关系，所以删除该记录
//		{
//			mapper.limit(1).deleteBy(criteria);
//			return true;
//		}
//	case RelationshipActionType::FriendRequest:
//		//这里没有检测是否已经是好友或者已经发送好友申请
//		if (ValidateIsFriend(dto.GetActorUid(),dto.GetReactorUid()))
//		{
//			return false;
//		}
//		if (ValidateHasRelationship(dto.GetActorUid(),dto.GetReactorUid()))
//		{
//			//这里只可能是被屏蔽或者已经发送过申请了，所以直接返回
//			return true;
//		}
//	case RelationshipActionType::FriendResponse:
//	{
//		if (!dto.GetAccept().has_value())
//		{
//			LOG_ERROR << "lack of field: is_accept";
//			return false;
//		}
//		if (dto.GetAccept().value())
//		{
//			std::string normalized_first_uid = dto.GetActorUid();
//			std::string	normalized_second_uid = dto.GetReactorUid();
//			//因为uid的长度是一致的，所以采用字典序比较
//			if (normalized_first_uid < normalized_second_uid)
//			{
//				std::swap(normalized_first_uid, normalized_second_uid);
//			}
//			Criteria criteria(Criteria(Relationships::Cols::_first_uid, CompareOperator::EQ, dto.GetReactorUid())
//				&& Criteria(Criteria(Relationships::Cols::_second_uid, CompareOperator::EQ, dto.GetReactorUid())));
//			mapper.limit(1).updateBy({ Relationships::Cols::_first_uid,Relationships::Cols::_second_uid,Relationships::Cols::_status },
//				criteria, normalized_first_uid,normalized_second_uid,"Friend");
//			return true;
//		}
//		else
//		{
//			Criteria criteria(Criteria(Relationships::Cols::_first_uid,CompareOperator::EQ,dto.GetReactorUid())
//			&&Criteria(Criteria(Relationships::Cols::_second_uid, CompareOperator::EQ, dto.GetReactorUid())));
//			mapper.limit(1).updateBy({ Relationships::Cols::_status },criteria,"refuse");
//			return true;
//		}
//	}
//		case RelationshipActionType::Unfriend:
//			std::string normalized_first_uid = dto.GetActorUid();
//			std::string	normalized_second_uid = dto.GetReactorUid();
//			//因为uid的长度是一致的，所以采用字典序比较
//			if (normalized_first_uid < normalized_second_uid)
//			{
//				std::swap(normalized_first_uid, normalized_second_uid);
//			}
//			Criteria criteria(Criteria(Relationships::Cols::_first_uid, CompareOperator::EQ, normalized_first_uid)
//				&& Criteria(Relationships::Cols::_second_uid, CompareOperator::EQ, normalized_second_uid));
//			auto result = mapper.limit(1).deleteBy(criteria);
//			return result == 1;
//	default:
//		LOG_ERROR << "unsupported type";
//		return false;
//	}
//}

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
			json_records.append(it->toJson());
		}
		return json_records;
	}
	
	//找到了，就从最新的往前返回message_number条
	auto records = mapper.orderBy(ChatRecord::Cols::_message_id, SortOrder::DESC).limit(num).findAll();
	Json::Value json_records(Json::arrayValue);
	for (auto it = records.rbegin(); it != records.rend(); ++it)
	{
		json_records.append(it->toJson());
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
		data.append(it->toJson());
	}
	LOG_INFO << "Get all records:\n" << data.toStyledString()<<"\n";
	return data;
}

Json::Value DatabaseManager::GetRelationships()
{
	Mapper<Relationships> mapper(GetDbClient());
	Json::Value data(Json::arrayValue);
	auto relationships = mapper.findAll();

	for (auto& relationship:relationships)
	{
		data.append(relationship.toJson());
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

bool DatabaseManager::GetUserInfoByAccount(const std::string& account,Json::Value& data)
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

bool DatabaseManager::GetUserNotification(const std::string& uid,Json::Value& data)
{
	Mapper<Notifications> mapper(GetDbClient());
	Criteria criteria(Notifications::Cols::_reactor_uid, uid);
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
	Criteria criteria(Users::Cols::_uid,CompareOperator::EQ,uid);
	size_t result = mapper.limit(1).updateBy({Users::Cols::_avatar},criteria,avatar);

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
	Criteria criteria(Users::Cols::_uid, CompareOperator::EQ,uid);
	auto result = mapper.limit(1).findBy(criteria);
	return (!result.empty());
}

bool DatabaseManager::ValidateRelationship(const std::string& actor_uid, const std::string& reactor_uid,
                                           const std::string& relationship)
{
	using Type = Utils::Relationship::StatusType;
	Mapper<Relationships> mapper(GetDbClient());
	switch (Utils::Relationship::StringToType(relationship))
	{
	case Type::Pending:
	case Type::Blocking:
		{
		Criteria criteria(Criteria(Relationships::Cols::_first_uid, CompareOperator::EQ, actor_uid)
			&& Criteria(Relationships::Cols::_second_uid, CompareOperator::EQ, reactor_uid)
			&& Criteria(Relationships::Cols::_status, CompareOperator::EQ, relationship));
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
			&& Criteria(Relationships::Cols::_status, CompareOperator::EQ, relationship));
		return !mapper.findBy(criteria).empty();
		}
	default:
		LOG_ERROR << "try to validate unknown type";
		return false;
	}
}




