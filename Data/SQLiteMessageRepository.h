#pragma once
#include "IMessageRepository.h"
#include "Common/AIMessage.h"

namespace drogon_model::sqlite3
{
	class Messages;
	class AiContext;
}

class ChatMessage;

class SQLiteMessageRepository :public IMessageRepository
{
	std::future<int64_t> RecordUserMessage(const ChatMessage& message) override;
	std::future<bool> RecordAIMessage(const AIMessage& message) override;
	std::future<Json::Value> GetMessageRecords(int thread_id, int64_t existed_id) override;
	std::future<Json::Value> GetAIContext(int thread_id, int64_t timestamp) override;
};
