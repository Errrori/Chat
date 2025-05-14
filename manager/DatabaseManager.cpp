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
		auto records = mapper.orderBy(ChatRecord::Cols::_message_id,SortOrder::DESC).findBy(criteria);
		return WriteRecordsReserveOrder(records, data);
	}
	
	//找到了，就从最新的往前返回message_number条
	auto records = mapper.orderBy(ChatRecord::Cols::_message_id, SortOrder::DESC).limit(num).findAll();
	return WriteRecordsReserveOrder(records, data);

}

Json::Value DatabaseManager::GetAllRecords(unsigned num)
{
	Mapper<ChatRecord> mapper(GetDbClient());
	Json::Value data(Json::arrayValue);
	auto records =
		mapper.orderBy(ChatRecord::Cols::_message_id, SortOrder::DESC).limit(num).findAll();

	for (auto it = records.rbegin(); it != records.rend(); ++it)
	{
		Json::Value json_record;
		json_record["message_id"] = std::to_string(it->getValueOfMessageId());
		json_record["sender_uid"] = it->getValueOfSenderUid();
		json_record["sender_name"] = it->getValueOfSenderName();
		json_record["content"] = it->getValueOfContent();
		json_record["create_time"] = it->getValueOfCreateTime();
		json_record["avatar"] = it->getValueOfAvatar();
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

Json::Value DatabaseManager::WriteRecordsReserveOrder(const std::vector<drogon_model::sqlite3::ChatRecords>& records,
                                                      Json::Value& data)
{
    for (auto it = records.rbegin(); it != records.rend(); ++it)
    {
        Json::Value json_record;
        json_record["message_id"] = std::to_string(it->getValueOfMessageId());
        json_record["sender_uid"] = it->getValueOfSenderUid();
        json_record["sender_name"] = it->getValueOfSenderName();
        json_record["content"] = it->getValueOfContent();
        json_record["create_time"] = it->getValueOfCreateTime();
        json_record["avatar"] = it->getValueOfAvatar();
        data.append(json_record);
    }
    return data;
}

