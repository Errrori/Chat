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

std::future<int64_t> SQLiteMessageRepository::RecordUserMessage(const ChatMessage& message)
{
	auto promise = std::make_shared<std::promise<int64_t>>();

	try
	{
		drogon_model::sqlite3::Messages db_message;

		db_message.setThreadId(static_cast<int64_t>(message.getThreadId()));
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

		db_message.setSenderUid(message.getSenderUid());
		db_message.setSenderName(message.getSenderName());
		db_message.setSenderAvatar(message.getSenderAvatar());
		//lack of status
		//db_message.setCreateTime(message.getCreateTime());
		//db_message.setUpdateTime(message.getUpdateTime());
		//message_id is auto-incremented in the database, so do not set it

		Mapper<Messages> mapper(DbAccessor::GetDbClient());
		mapper.insert(db_message,
			[promise](const Messages& record)
			{
				promise->set_value(record.getValueOfMessageId());
			},
			[promise](const DrogonDbException& e)
			{
				LOG_ERROR << "exception: " << e.base().what();
				promise->set_exception(std::make_exception_ptr(e.base()));
			});
	}catch (const std::exception& e)
	{
		LOG_ERROR << "exception: " << e.what();
		promise->set_exception(std::make_exception_ptr(e));
	}

	return promise->get_future();
}

std::future<bool> SQLiteMessageRepository::RecordAIMessage(const AIMessage& message)
{
	auto promise = std::make_shared<std::promise<bool>>();

	try
	{
		if (message.getMessageId().empty())
		{
			promise->set_value(false);
			return promise->get_future();
		}
		const auto& content = message.getContent();
		const auto& attachment = message.getAttachment();
		const auto& reasoning_content = message.getReasoningContent();

		if (content.empty()&&reasoning_content.empty())
		{
			promise->set_value(false);
			LOG_ERROR << "missing fields";
			return promise->get_future();
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

		Mapper<AiContext> mapper(DbAccessor::GetDbClient());
		mapper.insert(db_message,
			[promise](const AiContext& record)
			{
				promise->set_value(true);
			},
			[promise](const DrogonDbException& e)
			{
				promise->set_exception(std::make_exception_ptr(e.base()));
			});
	}
	catch (const std::exception& e)
	{
		promise->set_exception(std::make_exception_ptr(e));
	}

	return promise->get_future();
}

std::future<Json::Value> SQLiteMessageRepository::GetMessageRecords(int thread_id, int64_t existed_id)
{
	auto promise = std::make_shared<std::promise<Json::Value>>();
	
	Mapper<Messages> mapper(DbAccessor::GetDbClient());

	Criteria criteria;
	if (existed_id!=0)
		criteria = Criteria(Messages::Cols::_thread_id, CompareOperator::EQ, thread_id);
	else
		criteria = Criteria(Messages::Cols::_thread_id, CompareOperator::EQ, thread_id)
			&& Criteria(Messages::Cols::_message_id, CompareOperator::GT, existed_id);

	mapper.limit(50).findBy(criteria,
		[promise](const std::vector<Messages>& records)
		{
			Json::Value result(Json::arrayValue);
			for (const auto& record:records)
			{
				result.append(record.toJson());
			}
			promise->set_value(result);
		},
		[promise](const DrogonDbException& e)
		{
			LOG_ERROR << "exception: " << e.base().what();
			promise->set_exception(std::make_exception_ptr(e.base()));
		});

	return promise->get_future();
}

std::future<Json::Value> SQLiteMessageRepository::GetAIContext(int thread_id, int64_t timestamp)
{
	auto promise = std::make_shared<std::promise<Json::Value>>();

	Mapper<AiContext> mapper(DbAccessor::GetDbClient());

	Criteria criteria(Criteria(AiContext::Cols::_thread_id,CompareOperator::EQ,thread_id)
		&&Criteria(AiContext::Cols::_created_time,CompareOperator::GT,timestamp));

	//if (existed_id != 0)
	//	criteria = Criteria(AiContext::Cols::_thread_id, CompareOperator::EQ, thread_id);
	//else
	//	criteria = Criteria(AiContext::Cols::_thread_id, CompareOperator::EQ, thread_id)
	//	&& Criteria(AiContext::Cols::_message_id, CompareOperator::GT, existed_id);

	mapper.limit(50).findBy(criteria,
		[promise](const std::vector<AiContext>& records)
		{
			Json::Value result(Json::arrayValue);
			for (const auto& record : records)
			{
				result.append(record.toJson());
			}
			promise->set_value(result);
		},
		[promise](const DrogonDbException& e)
		{
			LOG_ERROR << "exception: " << e.base().what();
			promise->set_exception(std::make_exception_ptr(e.base()));
		});

	return promise->get_future();


}


