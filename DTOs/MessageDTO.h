#pragma once
#include "Utils.h"

using ContentType = Utils::Message::Content::ContentType;
using ChatType = Utils::Message::Chat::ChatType;

struct MessageDTO
{
public:
	void SetContentType(ContentType type);
	void SetChatType(ChatType type);
	void SetReceiverUid(const std::string& uid);
	void SetContent(const std::string& content);
	void SetConversationId(const std::string& id);

	ContentType GetContentType() const;
	ChatType GetChatType() const;
	std::string GetReceiverUid() const;
	std::string GetContent() const;
	std::optional<std::string> GetConversationId() const;

	void Clear();
	Json::Value TransToJsonMsg() const;
	std::optional<drogon_model::sqlite3::ChatRecords> ToChatRecords() const;
	std::optional<MessageDTO> BuildFromJson(const Json::Value& data); 

private:
	ContentType content_type = ContentType::Unknown;
	ChatType chat_type = ChatType::Unknown;
	std::string content;
	std::string receiver_uid;
	std::optional<std::string> conversation_id;
};

