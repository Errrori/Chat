#pragma once

class AIMessage;
class ChatMessage;

class IMessageRepository
{
public:
	virtual ~IMessageRepository() = default;
	virtual drogon::Task<int64_t> RecordUserMessage(const ChatMessage& message) = 0;
	virtual drogon::Task<> RecordAIMessage(const AIMessage& message) = 0;
	virtual drogon::Task<Json::Value> GetMessageRecords(int thread_id, int64_t existed_id = 0, int num = 50) = 0;
	virtual drogon::Task<Json::Value> GetAIContext(int thread_id, int64_t timestamp = 0) = 0;
};

