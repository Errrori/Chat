#pragma once
#include "models/Messages.h"

static const std::vector<std::string> AttachmentType = {"image","video","audio","file"};

class MessageManager
{
public:
	struct MsgData
	{
		int64_t message_id = 0;
		int thread_id = 0;
		std::string sender_uid = "";
		std::string sender_name = "";
		std::string sender_avatar = "";
		std::optional<std::string> content;
		std::optional<Json::Value> attachment;
		std::string create_time = "";
		std::string update_time = "";
		int status = 1; // 0=deleted, 1=normal


		static std::optional<MsgData> BuildFromJson(const Json::Value& json);

		std::optional<drogon_model::sqlite3::Messages> TransToMessages();
	};

	static bool PushMessage(const MsgData& msg);
	static std::optional<drogon_model::sqlite3::Messages> BuildMessage(const Json::Value& json);

	static Json::Value GetMessages(unsigned thread_id, int64_t existing_id, unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static Json::Value GetAllMessage(unsigned thread_id, unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
};

