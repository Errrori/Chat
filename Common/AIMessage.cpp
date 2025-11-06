#include "pch.h"
#include "AIMessage.h"
#include "models/AiContext.h"

AIMessage AIMessage::FromJson(const Json::Value& data)
{
	AIMessage message;

	try
	{
		if (data.isMember("thread_id"))
			message._thread_id = data["thread_id"].asInt();
		if (data.isMember("content"))
			message._content = data["content"].asString();
		if (data.isMember("attachment"))
			message._attachment = data["attachment"];
		if (data.isMember("created_time"))
			message._created_time = data["created_time"].asInt64();
		else
			message._created_time = Utils::GetCurrentTimeStamp();
		if (data.isMember("reasoning_content"))
			message._reasoning_content = data["reasoning_content"].asString();

		return message;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception in AIMessage::FromJson: " << e.what();
		return AIMessage{};
	}
}

std::string AIMessage::RoleToString(Role role)
{
	switch (role)
	{
	case Role::user:
		return "user";
	case Role::assistant:
		return "assistant";
	case Role::tool:
		return "tool";
	case Role::system:
		return "system";
	case Role::unknown:
	default:
		return "unknown";
	}
}

AIMessage::Role AIMessage::StringToRole(const std::string& role_str)
{
	if (role_str == "user") return Role::user;
	if (role_str == "assistant") return Role::assistant;
	if (role_str == "tool") return Role::tool;
	if (role_str == "system") return Role::system;
	return Role::unknown;
}

bool AIMessage::IsValid() const
{
	// AI消息至少需要: thread_id, content, role
	return _thread_id > 0 && !_content.empty() && _role != Role::unknown;
	// message_id.empty
}

std::optional<drogon_model::sqlite3::AiContext> AIMessage::ToDbObject() const
{
	if (!IsValid())
	{
		LOG_ERROR << "invalid AI message data";
		return std::nullopt;
	}

	drogon_model::sqlite3::AiContext dbObject;

	dbObject.setThreadId(static_cast<int64_t>(_thread_id));

	if (!_message_id .empty())
	{
		dbObject.setMessageId(_message_id);
	}

	if (!_content.empty())
		dbObject.setContent(_content);

	if (_attachment.has_value() && !_attachment->empty())
	{
		Json::StreamWriterBuilder builder;
		builder["indentation"] = "";
		std::string attachmentStr = Json::writeString(builder, _attachment.value());
		dbObject.setAttachment(attachmentStr);
	}

	dbObject.setRole(RoleToString(_role));

	if (!_reasoning_content.empty())
	{
		dbObject.setReasoningContent(_reasoning_content);
	}

	if (_created_time!=-1)
	{
		dbObject.setCreatedTime(_created_time);
	}

	return dbObject;
}
