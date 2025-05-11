#include "DatabaseManager.h"
#include "models/ChatRecords.h"
#include <drogon/orm/Mapper.h>

using namespace DataBase;
using ChatRecord = drogon_model::sqlite3::ChatRecords;
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
	//或许这里需要重构一条记录然后放入，不过出于简单易用的方面考虑直接放入消息
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

Json::Value DatabaseManager::GetChatRecords(unsigned num)
{
	Mapper<ChatRecord> mapper(GetDbClient());
	Json::Value data(Json::arrayValue);
	auto records = 
		mapper.orderBy(ChatRecord::Cols::_create_time,SortOrder::DESC).limit(num).findAll();
	
	for (const auto& record : records)
	{
		Json::Value json_record;
		json_record["message_id"] = record.getValueOfMessageId();
		json_record["sender_uid"] = record.getValueOfSenderUid();
		json_record["sender_name"] = record.getValueOfSenderName();
		json_record["content"] = record.getValueOfContent();
		json_record["create_time"] = record.getValueOfCreateTime();
		data.append(json_record);
	}

	LOG_INFO << "Get ChatRecords : "<< data.toStyledString();

	return data;
}

