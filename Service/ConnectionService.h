#pragma once
#include <unordered_map>
#include <drogon/WebSocketConnection.h>
class ChatMessage;
class Container;

class ConnectionService
{
	friend class Container;
public:

	ConnectionService(const ConnectionService& manager) = delete;
	ConnectionService& operator=(const ConnectionService& manager) = delete;
	ConnectionService(ConnectionService&& manager) = delete;
	ConnectionService& operator=(ConnectionService&& manager) = delete;

	bool AddConnection(const drogon::WebSocketConnectionPtr& conn,UserInfo info);

	bool RemoveConnection(const std::string& uid);
	bool RemoveConnection(const drogon::WebSocketConnectionPtr& conn);

	std::shared_ptr<UserInfo> GetConnInfo(const drogon::WebSocketConnectionPtr& conn) const;
	void Broadcast(const std::vector<std::string>& targets, const Json::Value& message);

	ConnectionService() = default;
	~ConnectionService() = default;

private:
	//id to connection
	std::unordered_map<std::string, drogon::WebSocketConnectionPtr> _conn_to_id_map;
	std::recursive_mutex _mutex;
};

