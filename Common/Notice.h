#pragma once
#include <json/json.h>
#include <string>
#include "Enums.h"

class Notice
{
public:
	Notice() = default;

	// 从Json解析
	static Notice FromJson(const Json::Value& json);

	// 转换为Json (包含外层的 type 和 data)
	Json::Value ToJson() const;

	bool IsValid() const;

	// Getters
	const std::string& getSenderUid() const { return _sender_uid; }
	const std::string& getSenderName() const { return _sender_name; }
	const std::string& getSenderAvatar() const { return _sender_avatar; }
	const std::string& getMessage() const { return _message; }
	int64_t getCreatedTime() const { return _created_time; }
	int64_t getEventId() const { return _event_id; }
	ChatEnums::NoticeType getType() const { return _type; }

	// Setters
	void setSenderUid(const std::string& uid) { _sender_uid = uid; }
	void setSenderName(const std::string& name) { _sender_name = name; }
	void setSenderAvatar(const std::string& avatar) { _sender_avatar = avatar; }
	void setMessage(const std::string& msg) { _message = msg; }
	void setCreatedTime(int64_t time) { _created_time = time; }
	void setEventId(int64_t event_id) { _event_id = event_id; }
	void setType(ChatEnums::NoticeType type) { _type = type; }

private:
	std::string _sender_uid;
	std::string _sender_name;
	std::string _sender_avatar;
	std::string _message;
	int64_t _created_time = -1;
	int64_t _event_id = -1;
	ChatEnums::NoticeType _type = ChatEnums::NoticeType::RequestReceived;
};
