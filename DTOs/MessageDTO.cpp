#include "pch.h"
#include "MessageDTO.h"
#include "models/ChatRecords.h"

using ChatRecords = drogon_model::sqlite3::ChatRecords;

std::optional<MessageDTO> MessageDTO::BuildFromJson(const Json::Value& data)
{
	if (!data.isMember("content_type") || !data.isMember("chat_type") || 
		!data.isMember("content") || !data.isMember("receiver_uid"))
	{
		return std::nullopt;
	}
	MessageDTO dto;

	dto.SetContentType(static_cast<ContentType>(data["content_type"].asInt()));
	dto.SetChatType(static_cast<ChatType>(data["chat_type"].asInt()));
	dto.SetContent(data["content"].asString());
	dto.SetReceiverUid(data["receiver_uid"].asString());

	if (data.isMember("conversation_id"))
	{
		dto.SetConversationId(data["conversation_id"].asString());
	}

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

ContentType MessageDTO::GetContentType() const
{
	return content_type;
}

ChatType MessageDTO::GetChatType() const
{
	return chat_type;
}

std::string MessageDTO::GetReceiverUid() const
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

Json::Value MessageDTO::TransToJsonMsg() const
{
	Json::Value data;
	data["content_type"] = static_cast<int>(content_type);
	data["chat_type"] = static_cast<int>(chat_type);
	data["content"] = content;
	data["receiver_uid"] = receiver_uid;
	if (conversation_id.has_value()) data["conversation_id"] = conversation_id.value();
	return data;
}

std::optional<ChatRecords> MessageDTO::ToChatRecords() const
{
	ChatRecords record;
	record.setContentType(Utils::Message::Content::TypeToString(content_type));
	record.setChatType(Utils::Message::Chat::TypeToString(chat_type));
	record.setContent(content);
	record.setReceiverUid(receiver_uid);
	if (conversation_id.has_value())
	{
		record.setConversationId(conversation_id.value());
	}
	return record;
}

void MessageDTO::Clear()
{
	content_type = ContentType::Unknown;
	chat_type = ChatType::Unknown;
	content.clear();
	receiver_uid.clear();
	conversation_id.reset();
}
