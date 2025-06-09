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
	void SetSenderUid(const std::string& uid);
	void SetSenderAvatar(const std::string& avatar);
	void SetSenderName(const std::string& name);
	void SetMessageId(int64_t id);
	void SetCreateTime(const std::string& time);

	ContentType GetContentType() const;
	ChatType GetChatType() const;
	std::string GetContent() const;
	std::optional<std::string> GetReceiverUid() const;
	std::optional<std::string> GetConversationId() const;
	std::string GetSenderUid() const;
	std::string GetSenderAvatar() const;
	std::string GetSenderName() const;
	int64_t GetMessageId() const;
	std::string GetCreateTime() const;

	void Clear();
	Json::Value TransToJsonMsg() const;
	std::optional<drogon_model::sqlite3::ChatRecords> TransToChatRecords() const;
	static std::optional<MessageDTO> BuildFromClientMsg(const Json::Value& data, std::string& error_msg); 

private:
	ContentType content_type = ContentType::Unknown;
	ChatType chat_type = ChatType::Unknown;
	std::string content;
	std::optional<std::string> receiver_uid;
	std::optional<std::string> conversation_id;

	std::string sender_uid;
	std::string sender_avatar;
	std::string sender_name;
	int64_t message_id = 0;
	std::string create_time;
};

