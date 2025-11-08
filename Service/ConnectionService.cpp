#include "pch.h"
#include "ConnectionService.h"

bool ConnectionService::AddConnection(const drogon::WebSocketConnectionPtr& conn,ConnInfo info)
{
	conn->setContext(std::make_shared<ConnInfo>(info));

	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(info.uid);
		if (it != _conn_to_id_map.end())
		{
			LOG_INFO << "user already have a connection";
			const auto& old_conn = it->second;
			if (old_conn&&old_conn->connected())
				old_conn->shutdown(drogon::CloseCode::kViolation,"another device log in");
			_conn_to_id_map.erase(it);
		}
		_conn_to_id_map.emplace(info.uid, conn);
		LOG_INFO << "new user connected: " << info.username;
		conn->setContext(std::make_shared<ConnInfo>(std::move(info)));
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

std::shared_ptr<ConnectionService::ConnInfo> ConnectionService::GetConnInfo(const drogon::WebSocketConnectionPtr& conn) const
{
	if (conn&&conn->connected())
		return conn->getContext<ConnInfo>();

	return nullptr;
}

void ConnectionService::Broadcast(const std::vector<std::string>& targets, const Json::Value& message)
{
	if (targets.empty())
	{
		LOG_INFO << "not target to send";
		return;
	}
	std::lock_guard lock(_mutex);

	for (const auto& uid : targets)
	{
		auto it = _conn_to_id_map.find(uid);
		if (it != _conn_to_id_map.end())
		{
			if (it->second && it->second->connected())
				it->second->sendJson(message);
			else
				_conn_to_id_map.erase(it);
		}
	}
}

