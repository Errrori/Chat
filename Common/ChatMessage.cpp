#include "pch.h"
#include "ChatMessage.h"
#include "models/Messages.h"
#include "WebMessageCodec.h"

ChatMessage ChatMessage::FromJson(const Json::Value& data)
{
	ChatMessage message;

	const auto& now = Utils::GetCurrentTimeStamp();

	try
	{
		if (data.isMember("thread_id")) 
			message._thread_id = data["thread_id"].asInt();
		if (data.isMember("message_id")) 
			message._message_id = data["message_id"].asInt64();
		if (data.isMember("content")) 
			message._content = data["content"].asString();
		if (data.isMember("attachment"))
			message._attachment = data["attachment"];
		if (data.isMember("sender_uid"))
			message._sender_uid = data["sender_uid"].asString();
		if (data.isMember("sender_name"))
			message._sender_name = data["sender_name"].asString();
		if (data.isMember("sender_avatar"))
			message._sender_avatar = data["sender_avatar"].asString();
		if (data.isMember("status"))
			message._status = data["status"].asInt();
		if (data.isMember("create_time"))
			message._create_time = data["create_time"].asInt64();
		else
			message._create_time = now; 
		if (data.isMember("update_time"))
			message._update_time = data["update_time"].asInt();
		else 
			message._update_time = now;

		return message;
	}catch (const std::exception& e)
	{
		LOG_ERROR << "exception in FromJson: " << e.what();
		return ChatMessage{};
	}
	
}

std::optional<drogon_model::postgres::Messages> ChatMessage::ToDbMessage() const
{
	if (!IsValid()) {
		LOG_ERROR << "invalid message data";
		return std::nullopt;
	}

	drogon_model::postgres::Messages dbMessage;

	dbMessage.setThreadId(static_cast<int64_t>(_thread_id));
	//message_id is auto-incremented in the database, so do not set it
	
	if (!_content.empty())
		dbMessage.setContent(_content);

	if (!_attachment.empty()) {
		Json::StreamWriterBuilder builder;
		builder["indentation"] = "";
		std::string attachmentStr = Json::writeString(builder, _attachment);
		dbMessage.setAttachment(attachmentStr);
	}
	
	if (!_sender_uid.empty()) {
		dbMessage.setSenderUid(_sender_uid);
	}
	
	if (!_sender_name.empty()) {
		dbMessage.setSenderName(_sender_name);
	}
	
	if (!_sender_avatar.empty()) {
		dbMessage.setSenderAvatar(_sender_avatar);
	}
	
	dbMessage.setStatus(static_cast<int64_t>(_status));
	
	if (_create_time>0) {
		dbMessage.setCreateTime(_create_time);
	}
	else
	{
		dbMessage.setCreateTime(Utils::GetCurrentTimeStamp());
	}
	
	if (_update_time>0) {
		dbMessage.setUpdateTime(_update_time);
	}
	else
	{
		dbMessage.setUpdateTime(Utils::GetCurrentTimeStamp());

	}
	return dbMessage;
}

std::optional<Json::Value> ChatMessage::ToMessage() const
{
	if (!IsValid()) {
		LOG_ERROR << "invalid message data";
		return std::nullopt;
	}
	return ChatCodec::EncodeChatEnvelope(*this);
}

bool ChatMessage::IsValid() const
{
	return _thread_id > 0 && (!_content.empty() || !_attachment.empty()) && !_sender_uid.empty();
}
