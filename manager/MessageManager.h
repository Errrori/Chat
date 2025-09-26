#pragma once
#include "models/Messages.h"

namespace ChatThreads
{
	struct ChatMessage;
}

static const std::vector<std::string> AttachmentType = {"image","video","audio","file"};

class MessageManager
{
public:
	std::optional<drogon_model::sqlite3::Messages> BuildMessages(const ChatThreads::ChatMessage& message);
	static std::optional<drogon_model::sqlite3::Messages> BuildMessage(const Json::Value& json);
	static std::optional<drogon_model::sqlite3::Messages>
		BuildMessage(const std::string& uid,const std::string& name,const std::string& avatar,const Json::Value& data);

	static Json::Value GetMessages(unsigned thread_id, int64_t existing_id, unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static Json::Value GetAllMessage(unsigned thread_id, unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
};

