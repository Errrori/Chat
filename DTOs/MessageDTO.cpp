#include "pch.h"
#include "MessageDTO.h"
#include "models/ChatRecords.h"

using ChatRecords = drogon_model::sqlite3::ChatRecords;
namespace Content = Utils::Message::Content;
namespace Chat = Utils::Message::Chat;

std::optional<MessageDTO> MessageDTO::BuildFromClientMsg(const Json::Value& data, std::string& error_msg)
{
	if (!data.isMember("content")||!data.isMember("sender_uid")
		||!data.isMember("sender_avatar")||!data.isMember("sender_name"))
	{
		error_msg = "missing field";
		return std::nullopt;
	}
	if (!Content::IsValid(data["content_type"].asString())
		|| !Chat::IsValid(data["chat_type"].asString()))
	{
		error_msg = "content/chat type is not valid";
		return std::nullopt;
	}

	if (!data.isMember("conversation_id") && !data.isMember("receiver_uid"))
	{
		error_msg = "message do not contain target id";
		return std::nullopt;
	}

	MessageDTO dto;
	dto.SetContentType(Content::StringToType(data["content_type"].asString()));
	dto.SetChatType(Chat::StringToType(data["chat_type"].asString()));
	dto.SetContent(data["content"].asString());
	dto.SetSenderName(data["sender_name"].asString());
	dto.SetSenderUid(data["sender_uid"].asString());
	dto.SetSenderAvatar(data["sender_avatar"].asString());
	dto.SetCreateTime(trantor::Date::now().toDbString());
	dto.SetMessageId(Utils::Message::GenerateMsgId());
	if (data.isMember("conversation_id"))
		dto.SetConversationId(data["conversation_id"].asString());
	if (data.isMember("receiver_uid"))
		dto.SetReceiverUid(data["receiver_uid"].asString());
		
	return dto;
}

void MessageDTO::SetContentType(ContentType type)
{
	content_type = type;
}

void MessageDTO::SetChatType(ChatType type)
{
	chat_type = type;
}

void MessageDTO::SetReceiverUid(const std::string& uid)
{
	receiver_uid = uid;
}

void MessageDTO::SetContent(const std::string& content)
{
	this->content = content;
}

void MessageDTO::SetConversationId(const std::string& id)
{
	conversation_id = id;
}

void MessageDTO::SetSenderUid(const std::string& uid)
{
	sender_uid = uid;
}

void MessageDTO::SetSenderAvatar(const std::string& avatar)
{
	sender_avatar = avatar;
}

void MessageDTO::SetSenderName(const std::string& name)
{
	sender_name = name;
}

void MessageDTO::SetMessageId(int64_t id)
{
	message_id = id;
}

void MessageDTO::SetCreateTime(const std::string& time)
{
	create_time = time;
}

ContentType MessageDTO::GetContentType() const
{
	return content_type;
}

ChatType MessageDTO::GetChatType() const
{
	return chat_type;
}

std::optional<std::string> MessageDTO::GetReceiverUid() const
{
	return receiver_uid;
}

std::string MessageDTO::GetContent() const
{
	return content;
}

std::optional<std::string> MessageDTO::GetConversationId() const
{
	return conversation_id;
}

std::string MessageDTO::GetSenderUid() const
{
	return sender_uid;
}

std::string MessageDTO::GetSenderAvatar() const
{
	return sender_avatar;
}

std::string MessageDTO::GetSenderName() const
{
	return sender_name;
}

int64_t MessageDTO::GetMessageId() const
{
	return message_id;
}

std::string MessageDTO::GetCreateTime() const
{
	return create_time;
}

Json::Value MessageDTO::TransToJsonMsg() const
{
	Json::Value data;
	data["content_type"] = Content::TypeToString(content_type);
	data["chat_type"] = Chat::TypeToString(chat_type);
	data["content"] = content;
	data["sender_uid"] = sender_uid;
	if (receiver_uid.has_value()) data["receiver_uid"] = receiver_uid.value();
	if (conversation_id.has_value()) data["conversation_id"] = conversation_id.value();
	data["sender_avatar"] = sender_avatar;
	data["sender_name"] = sender_name;
	data["message_id"] = Json::Value::Int64(message_id);
	return data;
}

std::optional<ChatRecords> MessageDTO::TransToChatRecords() const
{
	if (content_type == ContentType::Unknown)
		return std::nullopt;
	if (chat_type == ChatType::Unknown)
		return std::nullopt;
	if (sender_uid.empty() || sender_avatar.empty()||sender_uid.empty()
		||sender_name.empty()||message_id == 0||content.empty())
		return std::nullopt;
	//if both receiver_uid and conversation id are empty,this case it is wrong
	if (!conversation_id.has_value() && !receiver_uid.has_value())
		return std::nullopt;

	ChatRecords record;
	record.setContentType(Utils::Message::Content::TypeToString(content_type));
	record.setChatType(Utils::Message::Chat::TypeToString(chat_type));
	record.setContent(content);
	record.setCreateTime(create_time);
	record.setMessageId(message_id);
	record.setSenderAvatar(sender_avatar);
	record.setSenderName(sender_name);
	record.setSenderUid(sender_uid);
	if (conversation_id.has_value())
	{
		record.setConversationId(conversation_id.value());
	}
	if (receiver_uid.has_value())
	{
		record.setReceiverUid(receiver_uid.value());
	}
	return record;
}

void MessageDTO::Clear()
{
	content_type = ContentType::Unknown;
	chat_type = ChatType::Unknown;
	content.clear();
	receiver_uid.reset();
	conversation_id.reset();
	message_id = 0;
	sender_avatar.clear();
	sender_name.clear();
	sender_uid.clear();
	create_time.clear();
}
