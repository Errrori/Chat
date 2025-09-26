#include "pch.h"
#include "MessageManager.h"
#include "models/Messages.h"
#include "FileManager.h"
#include "ThreadManager.h"

using Criteria = drogon::orm::Criteria;
using Messages = drogon_model::sqlite3::Messages;
using CompareOperator = drogon::orm::CompareOperator;
using SortOrder = drogon::orm::SortOrder;
using drogon::orm::Mapper;



std::optional<drogon_model::sqlite3::Messages> MessageManager::BuildMessage(const Json::Value& json)
{
	//before use this function, make sure check all parameters
	try
	{
		LOG_INFO << "uid: " << json["sender_uid"].asString();
		Messages message;
		message.setAttachment(json["content"].asString());
		message.setCreateTime(json["create_time"].asString());
		message.setUpdateTime(json["update_time"].asString());
		message.setSenderAvatar(json["sender_avatar"].asString());
		message.setSenderName(json["sender_name"].asString());
		message.setSenderUid(json["sender_uid"].asString());
		message.setThreadId(json["thread_id"].asInt());

		LOG_INFO << "message built: " << message.toJson().toStyledString();
		return message;
	}
	catch (const drogon::orm::DrogonDbException& e)
	{
		LOG_ERROR << "Exception to build message from json: " << e.base().what();
		return std::nullopt;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "can not build message from json: " << e.what();
		return std::nullopt;
	}
}

std::optional<drogon_model::sqlite3::Messages> MessageManager::BuildMessage(const std::string& uid,
	const std::string& name, const std::string& avatar, const Json::Value& data)
{
	if (!data.isMember("content")||!data.isMember("thread_id"))
	{
		return std::nullopt;
	}

	const std::string current_time = trantor::Date::now().toDbString();
	Messages messages;
	messages.setSenderUid(uid);
	messages.setSenderName(name);
	messages.setSenderAvatar(avatar);
	messages.setThreadId(data["thread_id"].asInt());
	messages.setCreateTime(current_time);
	messages.setUpdateTime(current_time);

	try
	{
		messages.setContent(data["content"].asString());
	}catch (const std::exception&)
	{
		Json::StreamWriterBuilder builder;
		builder["indentation"] = "";
		messages.setContent(Json::writeString(builder,data["content"]));
	}
	messages.setAttachmentToNull();
	if (data.isMember("attachment"))
		messages.setAttachment(data["attachment"].asString());
	return messages;
}

Json::Value MessageManager::GetMessages(unsigned thread_id, int64_t existing_id, unsigned num)
{
	Mapper<drogon_model::sqlite3::Messages> mapper(DatabaseManager::GetDbClient());

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

