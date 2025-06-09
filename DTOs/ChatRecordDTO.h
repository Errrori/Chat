#pragma once
class ChatRecordDTO
{
public:
	std::optional<ChatRecordDTO> BuildFromJson(const Json::Value& data);
	std::optional<Json::Value> TransToJson() const;
	std::optional<drogon_model::sqlite3::ChatRecords> TransToChatRecords() const;

	void SetContent();
	void SetContentType();
	//...


private:
	std::string content;
	Utils::Message::Content::ContentType content_type = Utils::Message::Content::ContentType::Unknown;
	Utils::Message::Chat::ChatType chat_type = Utils::Message::Chat::ChatType::Unknown;
	std::string sender_uid;
	std::string sender_avatar;
	std::string sender_name;
	std::string message_id;
	std::string create_time;
	std::optional<std::string> receiver_uid;
	std::optional<std::string> conversation_id;
};

