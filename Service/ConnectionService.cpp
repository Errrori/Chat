#include "pch.h"
#include "ConnectionService.h"


bool ConnectionService::AddConnection(const drogon::WebSocketConnectionPtr& conn)
{
	const auto& user_info = conn->getContext<ConnInfo>();
	if (!user_info)
	{
		LOG_INFO << "can not get user info from connection context";
		return false;
	}
	
	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(user_info->uid);
		if (it != _conn_to_id_map.end())
		{
			LOG_ERROR << "user already have a connection";
			//这里说明之前这个用户连接已经存在了，需要关闭用户原来的连接，但是目前没有相关机制
			if (conn->connected())
				it->second->shutdown();
			_conn_to_id_map.erase(it);
		}
		_conn_to_id_map.emplace(user_info->uid, conn);
		LOG_INFO << "Add Connection,user name: " << user_info->username;
	}

	return true;
}

bool ConnectionService::RemoveConnection(const std::string& uid)
{
	std::lock_guard lock(_mutex);
	auto it = _conn_to_id_map.find(uid);
	if (it == _conn_to_id_map.end())
	{
		LOG_ERROR << "Can not find connection,uid :" << uid;
		return false;
	}

	_conn_to_id_map.erase(uid);

	LOG_INFO << "Remove Connection: " << uid;
	return true;
}

bool ConnectionService::RemoveConnection(const drogon::WebSocketConnectionPtr& conn)
{
	auto info_ptr = conn->getContext<Utils::UsersInfo>();
	if (info_ptr)
	{
		LOG_INFO << "Remove Connection: " << info_ptr->uid;
		return RemoveConnection(info_ptr->uid);
	}
	LOG_ERROR << "Error to get ConnectionPtr context";
	return false;
}

void ConnectionService::WriteConnInfo(const Utils::UsersInfo& info, const drogon::WebSocketConnectionPtr& conn)
{
	ConnInfo conn_info;
	conn_info.uid = info.uid;
	conn_info.account = info.account;
	conn_info.username = info.username;
	conn_info.avatar = info.avatar;
	conn->setContext(std::make_shared<ConnInfo>(std::move(conn_info)));
}


drogon::WebSocketConnectionPtr ConnectionService::GetConnection(const std::string& uid)
{
	std::lock_guard lock(_mutex);
	auto it = _conn_to_id_map.find(uid);
	if (it == _conn_to_id_map.end())
	{
		LOG_INFO << "can not find connection, uid:" << uid;
		return nullptr;
	}
	return it->second;
}

std::vector<drogon::WebSocketConnectionPtr> ConnectionService::GetConnections(
	const std::vector<std::string>& uids)
{
	std::lock_guard lock(_mutex);
	std::vector<drogon::WebSocketConnectionPtr> connections{};
	for (const auto& uid:uids)
	{
		auto it = _conn_to_id_map.find(uid);
		if (it!= _conn_to_id_map.end())
		{
			connections.emplace_back(it->second);
		}
	}
	return connections;
}

std::shared_ptr<ConnectionService::ConnInfo> ConnectionService::GetConnInfo(const drogon::WebSocketConnectionPtr& conn) const
{
	if (conn)
		return conn->getContext<ConnInfo>();

	return nullptr;
}

void ConnectionService::Send(const std::vector<std::string>& targets, const Json::Value& message)
{
	std::lock_guard lock(_mutex);
	if (targets.empty())
	{
		LOG_INFO << "send target is 0";
		return;
	}

	for (const auto& uid : targets)
	{
		auto it = _conn_to_id_map.find(uid);
		if (it != _conn_to_id_map.end())
			it->second->sendJson(message);
	}
}

