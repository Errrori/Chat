#include "pch.h"
#include "../models/ChatRecords.h"
#include "../models/Users.h"
#include "DatabaseManager.h"
#include <drogon/orm/Mapper.h>

using namespace DataBase;
using ChatRecord = drogon_model::sqlite3::ChatRecords;
using User = drogon_model::sqlite3::Users;
using namespace drogon::orm;

void DatabaseManager::InitDatabase()
{
	drogon::app().getDbClient()->execSqlSync(USER_TABLE);
	drogon::app().getDbClient()->execSqlSync(CHAT_RECORDS_TABLE);
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

void DatabaseManager::PushChatRecords(const Json::Value& message)
{
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

	//判断existing_id加上num条记录之后是否大于最新id
	//如果大于，返回从num开始之后的所有记录
	auto index_record = mapper.orderBy(ChatRecord::Cols::_message_id,SortOrder::DESC)
		.offset(num).limit(1).findAll();
	//LOG_INFO << "index record id : " << index_record[0].getValueOfMessageId();
	if (index_record.empty()|| (!index_record.empty()&&index_record[0].getValueOfMessageId()>existing_id))
	{
		auto newest_record = mapper.orderBy(ChatRecord::Cols::_message_id, SortOrder::DESC).limit(1).findAll();
		Criteria criteria(ChatRecord::Cols::_message_id, CompareOperator::LE,newest_record[0].getValueOfMessageId());
		auto records = mapper.orderBy(ChatRecord::Cols::_message_id,SortOrder::DESC).limit(num).findBy(criteria);
		Json::Value data(Json::arrayValue);
		for (const auto& record : records)
		{
			Json::Value json_record;
			json_record["message_id"] = std::to_string(record.getValueOfMessageId());
			json_record["sender_uid"] = record.getValueOfSenderUid();
			json_record["sender_name"] = record.getValueOfSenderName();
			json_record["content"] = record.getValueOfContent();
			json_record["create_time"] = record.getValueOfCreateTime();
			json_record["avatar"] = record.getValueOfAvatar();
			data.append(json_record);
		}
		return data;
	}
	//如果小于,从最新的消息开始返回num条
	return GetAllRecords(num);
}

//Json::Value DatabaseManager::GetChatRecords(int64_t existing_id,unsigned num)
//{
//	Mapper<ChatRecord> mapper(GetDbClient());
//
//	//如果现有id不存在，直接返回
//	auto exist_record = 
//		mapper.limit(1).findBy(Criteria(ChatRecord::Cols::_message_id,CompareOperator::EQ,existing_id));
//	if (exist_record.empty())
//	{
//		LOG_ERROR << "can not find existing id:" << existing_id;
//		return Json::nullValue;
//	}
//
//	////如果数量大于数据库总数
//	//if (num> mapper.count())
//	//{
//	//	return GetAllRecords(mapper.count());
//	//}
//
//	//获取最新的id开始往前第num条的数据
//	auto target_record = mapper.orderBy(ChatRecord::Cols::_message_id, SortOrder::DESC)
//		.limit(1).offset(num).findAll();
//
//	//如果表中数据不足num条
//	if (target_record.empty())
//	{
//		auto newest_record =
//			mapper.orderBy(ChatRecord::Cols::_message_id, SortOrder::DESC).limit(1).findAll();
//
//		auto newest_message_id = newest_record[0].getValueOfMessageId();
//
//		Criteria criteria(Criteria(ChatRecord::Cols::_message_id, CompareOperator::LT, newest_message_id) &&
//			Criteria(ChatRecord::Cols::_message_id, CompareOperator::GT, existing_id));
//
//		auto records = mapper.findBy(criteria);
//
//		Json::Value data(Json::arrayValue);
//		for (const auto& record : records)
//		{
//			Json::Value json_record;
//			json_record["message_id"] = record.getValueOfMessageId();
//			json_record["sender_uid"] = record.getValueOfSenderUid();
//			json_record["sender_name"] = record.getValueOfSenderName();
//			json_record["content"] = record.getValueOfContent();
//			json_record["create_time"] = record.getValueOfCreateTime();
//			json_record["avatar"] = record.getValueOfAvatar();
//			data.append(json_record);
//		}
//		return data;
//	}
//
//	//找到了对应的第num条消息
//	auto target_message_id = target_record[0].getValueOfMessageId();
//
//	//对比目标id和现有的id
//	if (target_message_id>existing_id)//目标id大于现有id，说明比现有id记录更晚，从最新的id开始向前返回num条
//	{
//		return GetAllRecords(num);
//	}
//	//目标id小于现有id，说明比现有id记录更早，返回最新的id到现有id（包括最新id不包括现有id）的记录
//	auto newest_record = 
//		mapper.orderBy(ChatRecord::Cols::_message_id, SortOrder::DESC).limit(1).findAll();
//
//	auto newest_message_id = newest_record[0].getValueOfMessageId();
//
//	Criteria criteria(Criteria(ChatRecord::Cols::_message_id, CompareOperator::LT, newest_message_id) &&
//		Criteria(ChatRecord::Cols::_message_id, CompareOperator::GT, existing_id));
//
//	auto records = mapper.findBy(criteria);
//
//	Json::Value data(Json::arrayValue);
//	for (const auto& record:records)
//	{
//		Json::Value json_record;
//		json_record["message_id"] = record.getValueOfMessageId();
//		json_record["sender_uid"] = record.getValueOfSenderUid();
//		json_record["sender_name"] = record.getValueOfSenderName();
//		json_record["content"] = record.getValueOfContent();
//		json_record["create_time"] = record.getValueOfCreateTime();
//		json_record["avatar"] = record.getValueOfAvatar();
//		data.append(json_record);
//	}
//	return data;
//}

Json::Value DatabaseManager::GetAllRecords(unsigned num)
{
	Mapper<ChatRecord> mapper(GetDbClient());
	Json::Value data(Json::arrayValue);
	auto records =
		mapper.orderBy(ChatRecord::Cols::_create_time, SortOrder::DESC).limit(num).findAll();

	for (const auto& record : records)
	{
		Json::Value json_record;
		json_record["message_id"] = std::to_string(record.getValueOfMessageId());
		json_record["sender_uid"] = record.getValueOfSenderUid();
		json_record["sender_name"] = record.getValueOfSenderName();
		json_record["content"] = record.getValueOfContent();
		json_record["create_time"] = record.getValueOfCreateTime();
		json_record["avatar"] = record.getValueOfAvatar();
		data.append(json_record);
	}
	LOG_INFO << "Get all records:\n" << data.toStyledString()<<"\n";
	return data;
}


bool DatabaseManager::ModifyAvatar(const std::string& uid, const std::string& avatar)
{
	Criteria criteria(User::Cols::_uid,CompareOperator::EQ,uid);
	Mapper<User> mapper(DatabaseManager::GetDbClient());
	auto record = mapper.findOne(criteria);
	mapper.limit(1);
	User user;
	user.setId(record.getPrimaryKey());
	user.setAvatar(avatar);
	size_t result = mapper.updateBy({User::Cols::_avatar},criteria,avatar);

	return result == 1;
}

