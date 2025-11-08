#include "pch.h"
#include "SQLiteMessageRepository.h"

#include "DbAccessor.h"
#include "Common/ChatMessage.h"
#include "models/Messages.h"
#include "models/AiContext.h"
using namespace drogon::orm;
using Messages = drogon_model::sqlite3::Messages;
using AiContext = drogon_model::sqlite3::AiContext;

//检查的逻辑放在Service层，需要检查发送者是否在该thread中
//返回值为message_id

drogon::Task<int64_t> SQLiteMessageRepository::RecordUserMessage(const ChatMessage& message)
{
	try
	{
		drogon_model::sqlite3::Messages db_message;

		//message_id is auto-incremented in the database, so do not set it

		const auto& content = message.getContent();
		const auto& attachment = message.getAttachment();

		if (!content.empty())
			db_message.setContent(content);

		if (!attachment.empty()) {
			Json::StreamWriterBuilder builder;
			builder["indentation"] = "";
			std::string attachmentStr = Json::writeString(builder, attachment);
			db_message.setAttachment(attachmentStr);
		}

		if (attachment.empty() && content.empty())
			throw std::invalid_argument("content and attachment are both empty");

		db_message.setSenderUid(message.getSenderUid());
		db_message.setSenderName(message.getSenderName());
		db_message.setSenderAvatar(message.getSenderAvatar());
		db_message.setCreateTime(message.getCreateTime());
		db_message.setUpdateTime(message.getUpdateTime());

		CoroMapper<Messages> mapper(DbAccessor::GetDbClient());
		const auto& coro_msg = co_await mapper.insert(db_message);
		co_return coro_msg.getValueOfMessageId();
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception: " << e.what();
		throw;
	}
}


drogon::Task<> SQLiteMessageRepository::RecordAIMessage(const AIMessage& message)
{
	try
	{
		if (message.getMessageId().empty())
			throw std::invalid_argument("message id is empty");
		
		const auto& content = message.getContent();
		const auto& attachment = message.getAttachment();
		const auto& reasoning_content = message.getReasoningContent();

		if (content.empty()&&reasoning_content.empty())
		{
			throw std::invalid_argument("message id is empty");
		}
		drogon_model::sqlite3::AiContext db_message;
		if (!content.empty())
			db_message.setContent(content);
		if (!reasoning_content.empty())
			db_message.setReasoningContent(message.getReasoningContent());
		if (attachment.has_value()&&!attachment.value().empty()) {
			Json::StreamWriterBuilder builder;
			builder["indentation"] = "";
			std::string attachmentStr = Json::writeString(builder, attachment.value());
			db_message.setAttachment(attachmentStr);
		}

		db_message.setMessageId(message.getMessageId());
		db_message.setThreadId(static_cast<int64_t>(message.getThreadId()));
		db_message.setRole(AIMessage::RoleToString(message.getRole()));
		db_message.setCreatedTime(message.getCreatedTime());

		CoroMapper<AiContext> mapper(DbAccessor::GetDbClient());
		co_await mapper.insert(db_message);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception: " << e.what();
		throw;
	}
}

drogon::Task<Json::Value> SQLiteMessageRepository::GetMessageRecords(int thread_id, int64_t existed_id, int num)
{
	try
	{
		CoroMapper<Messages> mapper(DbAccessor::GetDbClient());

		Criteria criteria;
		if (existed_id != 0)
			criteria = Criteria(Messages::Cols::_thread_id, CompareOperator::EQ, thread_id);
		else
			criteria = Criteria(Messages::Cols::_thread_id, CompareOperator::EQ, thread_id)
			&& Criteria(Messages::Cols::_message_id, CompareOperator::GT, existed_id);

		const auto& records = co_await mapper.limit(num).findBy(criteria);

		Json::Value json_records;

		for (const auto& record : records)
			json_records.append(record.toJson());

		co_return json_records;
	}catch (const std::exception& e)
	{
		LOG_ERROR << "exception: " << e.what();
		throw;
	}
	
}

drogon::Task<Json::Value> SQLiteMessageRepository::GetAIContext(int thread_id, int64_t timestamp)
{
	try
	{
		CoroMapper<AiContext> mapper(DbAccessor::GetDbClient());

		Criteria criteria(Criteria(AiContext::Cols::_thread_id, CompareOperator::EQ, thread_id)
			&& Criteria(AiContext::Cols::_created_time, CompareOperator::GT, timestamp));

		auto context = co_await mapper.limit(20).findBy(criteria);

		Json::Value json_records;

		for (const auto& record : context)
			json_records.append(record.toJson());

		co_return json_records;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception: " << e.what();
		throw;
	}
	
}


