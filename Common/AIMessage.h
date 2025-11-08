#pragma once
#include <json/json.h>
#include <string>
#include <optional>

namespace drogon_model::sqlite3
{
	class AiContext;
}

class AIMessage
{
public:
	enum class Role : std::int8_t
	{
		user = 0,
		assistant,
		tool,
		system,
		unknown
	};
	AIMessage() = default;
	
	AIMessage(int thread_id,const std::string& message_id,
		const std::string& content,int64_t create_time,Role role = Role::unknown)
		:_thread_id(thread_id), _message_id(message_id), _content(content), _created_time(create_time), _role(role){}

	static AIMessage FromJson(const Json::Value& data);
	static std::string RoleToString(Role role);
	static Role StringToRole(const std::string& role_str);

	bool IsValid() const;
	std::optional<drogon_model::sqlite3::AiContext> ToDbObject() const;

	// Getters
	int getThreadId() const { return _thread_id; }
	std::string getMessageId() const { return _message_id; }
	const std::string& getContent() const { return _content; }
	const std::optional<Json::Value>& getAttachment() const { return _attachment; }
	Role getRole() const { return _role; }
	int64_t getCreatedTime() const { return _created_time; }
	const std::string& getReasoningContent() const { return _reasoning_content; }

	// Setters
	void setThreadId(int thread_id) { _thread_id = thread_id; }
	void setMessageId(const std::string& id) { _message_id = id; }
	void setContent(const std::string& content) { _content = content; }
	void setAttachment(const Json::Value& attachment) { _attachment = attachment; }
	void setRole(Role role) { _role = role; }
	void setCreatedTime(int64_t time) { _created_time = time; }
	void setReasoningContent(const std::string& reasoning) { _reasoning_content = reasoning; }

private:
	int _thread_id = -1;
	std::string _message_id;
	std::string _content;
	std::string _reasoning_content;
	std::optional<Json::Value> _attachment;
	int64_t _created_time = -1;
	Role _role{ Role::unknown };
};

