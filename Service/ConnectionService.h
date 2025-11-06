#pragma once
#include <unordered_map>
#include <drogon/WebSocketConnection.h>
class ChatMessage;
class Container;

class ConnectionService
{
	friend class Container;
public:
	struct ConnInfo
	{
		std::string uid;
		std::string account;
		std::string username;
		std::string avatar;
	};

	ConnectionService(const ConnectionService& manager) = delete;
	ConnectionService& operator=(const ConnectionService& manager) = delete;
	ConnectionService(ConnectionService&& manager) = delete;
	ConnectionService& operator=(ConnectionService&& manager) = delete;

	bool AddConnection(const drogon::WebSocketConnectionPtr& conn);
	bool RemoveConnection(const std::string& uid);
	bool RemoveConnection(const drogon::WebSocketConnectionPtr& conn);
	void WriteConnInfo(const Utils::UsersInfo& info, const drogon::WebSocketConnectionPtr& conn);

	drogon::WebSocketConnectionPtr GetConnection(const std::string& uid);
	std::vector<drogon::WebSocketConnectionPtr> GetConnections(const std::vector<std::string>& uids);
	std::shared_ptr<ConnInfo> GetConnInfo(const drogon::WebSocketConnectionPtr& conn) const;

	void Send(const std::vector<std::string>& targets, const Json::Value& message);

	ConnectionService() = default;
	~ConnectionService() = default;

private:
	//id to connection
	std::unordered_map<std::string, drogon::WebSocketConnectionPtr> _conn_to_id_map;
	std::recursive_mutex _mutex;
};

