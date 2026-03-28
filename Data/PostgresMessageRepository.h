#pragma once
#include "IMessageRepository.h"
#include <drogon/orm/DbClient.h>

namespace drogon_model::postgres
{
	class Messages;
	class AiContext;
}

class ChatMessage;

class PostgresMessageRepository :public IMessageRepository
{
public:
	explicit PostgresMessageRepository(drogon::orm::DbClientPtr db) : _db(std::move(db)) {}
	drogon::Task<int64_t> RecordUserMessage(const ChatMessage& message) override;
	drogon::Task<> RecordAIMessage(const AIMessage& message) override;
	drogon::Task<Json::Value> GetMessageRecords(int thread_id, int64_t existed_id, int num = 50) override;
	drogon::Task<Json::Value> GetAIContext(int thread_id, int64_t timestamp) override;

	drogon::Task<Json::Value> GetChatOverviews(int64_t existing_id,const std::string& uid) override;

private:
	drogon::orm::DbClientPtr _db;
};
