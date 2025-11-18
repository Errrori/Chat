#include "pch.h"
#include "ConnectionService.h"
#include "Common/User.h"

bool ConnectionService::AddConnection(const drogon::WebSocketConnectionPtr& conn,UserInfo info)
{
	conn->setContext(std::make_shared<UserInfo>(info));

	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(info.getUid());
		if (it != _conn_to_id_map.end())
		{
			LOG_INFO << "user already have a connection";
			const auto& old_conn = it->second;
			if (old_conn&&old_conn->connected())
				old_conn->shutdown(drogon::CloseCode::kViolation,"another device log in");
			_conn_to_id_map.erase(it);
		}
		_conn_to_id_map.emplace(info.getUid(), conn);
		LOG_INFO << "new user connected: " << info.getUsername();
		conn->setContext(std::make_shared<UserInfo>(std::move(info)));
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
	auto info_ptr = conn->getContext<UserInfo>();
	if (info_ptr)
	{
		LOG_INFO << "Remove Connection: " << info_ptr->getUid();
		return RemoveConnection(info_ptr->getUid());
	}
	LOG_ERROR << "Error to get ConnectionPtr context";
	return false;
}

std::shared_ptr<UserInfo> ConnectionService::GetConnInfo(const drogon::WebSocketConnectionPtr& conn) const
{
	if (conn&&conn->connected())
		return conn->getContext<UserInfo>();

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
                Utils::SendJson(it->second, message);
            else
                _conn_to_id_map.erase(it);
		}
	}
}

