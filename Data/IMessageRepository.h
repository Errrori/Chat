#pragma once

class AIMessage;
class ChatMessage;

class IMessageRepository
{
public:
	virtual ~IMessageRepository() = default;
	virtual std::future<int64_t> RecordUserMessage(const ChatMessage& message) = 0;
	virtual std::future<bool> RecordAIMessage(const AIMessage& message) = 0;
	virtual std::future<Json::Value> GetMessageRecords(int thread_id, int64_t existed_id = 0) = 0;
	virtual std::future<Json::Value> GetAIContext(int thread_id, int64_t timestamp = 0) = 0;
};

