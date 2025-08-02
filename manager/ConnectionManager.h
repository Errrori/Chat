#pragma once
#include <string>
#include <unordered_map>
#include <drogon/drogon.h>
#include <drogon/WebSocketConnection.h>
#include "Utils.h"


class ConnectionManager
{
public:
	ConnectionManager(const ConnectionManager& manager) = delete;
	ConnectionManager& operator=(const ConnectionManager& manager) = delete;
	ConnectionManager(ConnectionManager&& manager) = delete;
	ConnectionManager& operator=(ConnectionManager&& manager) = delete;

	static ConnectionManager& GetInstance();
	bool AddConnection(const Utils::UserInfo& info, const drogon::WebSocketConnectionPtr& conn);
	bool RemoveConnection(const std::string& uid);
	bool RemoveConnection(const drogon::WebSocketConnectionPtr& conn);
	bool AddUIdToNameRef(const std::string& uid, const std::string& name);
	void BroadcastMsg(const std::string& uid, const Json::Value& msg);
	void BroadcastMsg(const std::string& uid, const std::string& msg);
	Json::Value GetOnlineUsers();

	std::string GetName(const std::string& uid);
	std::optional<drogon::WebSocketConnectionPtr> GetConnection(const std::string& uid);
	std::vector<drogon::WebSocketConnectionPtr> GetConnection(const std::vector<std::string>& uids);

private:
	ConnectionManager() = default;
	~ConnectionManager() = default;

private:
	//id to username
	std::unordered_map<std::string, std::string> _name_map;
	//id to connection
	std::unordered_map<std::string, drogon::WebSocketConnectionPtr> _conn_map;
	std::mutex _conn_mtx;
	std::mutex _name_mtx;
};

