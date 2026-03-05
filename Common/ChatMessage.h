#pragma once
#include <json/json.h>
#include <string>

#include "ChatThread.h"

namespace drogon_model::sqlite3
{
	class Messages;
}

class ChatMessage
{
public:
	static ChatMessage FromJson(const Json::Value& data);
	std::optional<drogon_model::sqlite3::Messages> ToDbMessage() const;
	std::optional<Json::Value> ToMessage() const;
	bool IsValid() const;


	// Getters
	int getThreadId() const { return _thread_id; }
	int64_t getMessageId() const { return _message_id; }
	const std::string& getContent() const { return _content; }
	const Json::Value& getAttachment() const { return _attachment; }
	const std::string& getSenderUid() const { return _sender_uid; }
	const std::string& getSenderName() const { return _sender_name; }
	const std::string& getSenderAvatar() const { return _sender_avatar; }
	int getStatus() const { return _status; }
	int64_t getCreateTime() const { return _create_time; }
	int64_t getUpdateTime() const { return _update_time; }

	// Setters
	void setThreadId(int thread_id) { _thread_id = thread_id; }
	void setMessageId(int64_t id) { _message_id = id; }
	void setContent(const std::string& content) { _content = content; }
	void setAttachment(const Json::Value& attachment) { _attachment = attachment; }
	void setSenderUid(const std::string& uid) { _sender_uid = uid; }
	void setSenderName(const std::string& name) { _sender_name = name; }
	void setSenderAvatar(const std::string& avatar) { _sender_avatar = avatar; }
	void setStatus(int s) { _status = s; }
	void setCreateTime(int64_t time) { _create_time = time; }
	void setUpdateTime(int64_t time) { _update_time = time; }

private:
	int _thread_id = -1;
	int64_t _message_id = -1;
	std::string _content;
	Json::Value _attachment;
	std::string _sender_uid;
	std::string _sender_name;
	std::string _sender_avatar;
	int _status = 1;
	int64_t _create_time = -1;
	int64_t _update_time = -1;
};

