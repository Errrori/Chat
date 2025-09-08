#include "pch.h"
#include "MessageManager.h"
#include "models/Messages.h"
#include "FileManager.h"

using Criteria = drogon::orm::Criteria;
using Messages = drogon_model::sqlite3::Messages;
using CompareOperator = drogon::orm::CompareOperator;
using SortOrder = drogon::orm::SortOrder;
using drogon::orm::Mapper;

bool MessageManager::ValidateMsg(const MsgData& msg)
{
	if (!DatabaseManager::ValidateThreadId(msg.thread_id))
	{
		LOG_ERROR << "Thread id is not valid: " << msg.thread_id;
		return false;
	}

	LOG_INFO << "attachment: " << (msg.attachment.has_value() ? msg.attachment.value().toStyledString() : "none");

	if(msg.attachment.has_value())
	{
		const auto& attachment_json = msg.attachment.value();
		if(!attachment_json.isMember("type") || !attachment_json.isMember("file_path"))
		{
			LOG_ERROR<<"Attachment json is not valid: " << msg.attachment.value().toStyledString();
			return false;
		}
		const auto& type = attachment_json["type"].asString();
		const auto& file_path = attachment_json["file_path"].asString();
		if(type=="image")
		{
			return FileManager::CheckFileExists(file_path);
		}

		return false;
	}

	//file dont have attachment,check if content is valid
	//...
	return true;

}

bool MessageManager::PushMessage(const MsgData& msg)
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
	message.setAttachment(msg.attachment.value_or("").toStyledString());

	Mapper<Messages> mapper(DatabaseManager::GetDbClient());
	mapper.insert(message,
		[](const Messages& data)
		{
			LOG_INFO << "Insert new message successful";
		},
		[](const drogon::orm::DrogonDbException& e)
		{
			LOG_ERROR << "Exception to insert message: " << e.base().what();
		});
	return true;
}

Json::Value MessageManager::GetMessages(unsigned thread_id, int64_t existing_id, unsigned num)
{
	drogon::orm::Mapper<drogon_model::sqlite3::Messages> mapper(DatabaseManager::GetDbClient());

	// �������id�����ڣ�ֱ�ӷ���
	auto exist_record =
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
	auto index_record = mapper.orderBy(Messages::Cols::_message_id, SortOrder::ASC)
		.offset(num).limit(1).findBy(criteria);
	auto latest_record = mapper.orderBy(Messages::Cols::_message_id, SortOrder::DESC).limit(1).findAll();
	auto latest_id = latest_record[0].getValueOfMessageId();
	Json::Value data(Json::arrayValue);

	// �Ҳ������ʹ�������е�λ�ÿ�ʼ��󷵻ص����µ�
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

	auto records = mapper.orderBy(Messages::Cols::_message_id, SortOrder::DESC).limit(num)
		.findBy(Criteria(Messages::Cols::_thread_id, CompareOperator::EQ, thread_id));
	Json::Value json_records(Json::arrayValue);
	for (auto it = records.rbegin(); it != records.rend(); ++it)
	{
		json_records.append(it->toJson());
	}
	return json_records;
}

Json::Value MessageManager::GetAllMessage(unsigned thread_id, unsigned num)
{
	Mapper<Messages> mapper(DatabaseManager::GetDbClient());
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

std::optional<MessageManager::MsgData> MessageManager::MsgData::BuildFromJson(const Json::Value& json) {
	MsgData msg{};

	if (!json.isMember("message_id")||!json.isMember("sender_uid")||
		!json.isMember("thread_id")||(!json.isMember("content")&&!json.isMember("attachment")))
		return std::nullopt;

	msg.sender_uid = json["sender_uid"].asString();
	msg.message_id = std::stoll(json["message_id"].asString());
	msg.thread_id = json["thread_id"].asInt();

	if(json.isMember("sender_uid"))
		msg.sender_uid = json["sender_uid"].asString();
	if(json.isMember("sender_name"))
		msg.sender_name = json["sender_name"].asString();
	if(json.isMember("sender_avatar"))
		msg.sender_avatar = json["sender_avatar"].asString();
	if(json.isMember("content"))
		msg.content = json["content"].asString();
	if (json.isMember("attachment") && !json["attachment"].isNull() && !json["attachment"].asString().empty()) 
		msg.attachment = json["attachment"];

	if(json.isMember("create_time"))
		msg.create_time = json["create_time"].asString();
	if(json.isMember("update_time"))
		msg.update_time = json["update_time"].asString();
	if(json.isMember("status"))
		msg.status = json["status"].asInt();
	return msg;
}


std::optional<drogon_model::sqlite3::Messages> MessageManager::MsgData::TransToMessages()
{
	if(!message_id && !thread_id && sender_uid.empty() 
		&& sender_name.empty() && sender_avatar.empty() 
		&& create_time.empty() && update_time.empty() )
		return std::nullopt;

	Messages message;
	message.setMessageId(message_id);
	message.setThreadId(thread_id);
	message.setSenderUid(sender_uid);
	message.setSenderName(sender_name);
	message.setSenderAvatar(sender_avatar);
	message.setContent(content.value_or(""));
	message.setAttachment(attachment.has_value() ? attachment.value().toStyledString() : "");
	message.setCreateTime(create_time);
	message.setUpdateTime(update_time);
	message.setStatus(status);
	return message;
}

