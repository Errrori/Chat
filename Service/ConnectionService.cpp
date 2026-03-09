#include "pch.h"
#include "ConnectionService.h"
#include "RedisService.h"
#include "Common/User.h"

drogon::Task<bool> ConnectionService::IsOnline(const std::string& uid)
{
	{
		std::lock_guard lock(_mutex);
		if (_conn_to_id_map.contains(uid))
			co_return true;
	}
	co_return co_await _redis_service->IsOnline(uid);
}

bool ConnectionService::AddConnection(const drogon::WebSocketConnectionPtr& conn, UserInfo info)
{
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

	const auto uid = conn->getContext<UserInfo>()->getUid();
	drogon::async_run([this, uid]() -> drogon::Task<> {
		co_await OnUserConnected(uid);
	});

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

	drogon::async_run([this, uid]() -> drogon::Task<> {
		co_await OnUserDisconnected(uid);
	});

	return true;
}


drogon::Task<> ConnectionService::OnUserConnected(std::string uid)
{
	co_await _redis_service->SetOnline(uid);
	auto msgs = co_await _redis_service->PopAllOfflineMessages(uid);
	if (!msgs.empty())
	{
		LOG_INFO << "Delivering " << msgs.size() << " offline messages to " << uid;
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		if (it != _conn_to_id_map.end() && it->second && it->second->connected())
		{
			for (const auto& msg_str : msgs)
				it->second->send(msg_str);
		}
	}
	else
	{
		LOG_INFO << "you don't have offline message";
	}
}

drogon::Task<> ConnectionService::OnUserDisconnected(std::string uid)
{
	co_await _redis_service->SetOffline(uid);
}

bool ConnectionService::RemoveConnection(const drogon::WebSocketConnectionPtr& conn)
{
	auto info_ptr = conn->getContext<UserInfo>();
	if (!info_ptr)
	{
		LOG_ERROR << "Error to get ConnectionPtr context";
		return false;
	}

	const auto uid = info_ptr->getUid();
	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		// 只有当 map 里存的就是这个连接时才删除，防止重连时把新连接误删
		if (it == _conn_to_id_map.end() || it->second != conn)
		{
			LOG_INFO << "Connection already replaced, skip remove for uid: " << uid;
			return false;
		}
		_conn_to_id_map.erase(it);
	}

	LOG_INFO << "Remove Connection: " << uid;
	drogon::async_run([this, uid]() -> drogon::Task<> {
		co_await OnUserDisconnected(uid);
	});
	return true;
}

std::shared_ptr<UserInfo> ConnectionService::GetConnInfo(const drogon::WebSocketConnectionPtr& conn) const
{
	if (conn&&conn->connected())
		return conn->getContext<UserInfo>();

	return nullptr;
}

bool ConnectionService::SendIfConnected(const std::string& uid, const Json::Value& message)
{
	std::lock_guard lock(_mutex);
	auto it = _conn_to_id_map.find(uid);
	if (it == _conn_to_id_map.end())
		return false;

	if (!(it->second && it->second->connected()))
	{
		_conn_to_id_map.erase(it);
		return false;
	}

	Utils::SendJson(it->second, message);
	return true;
}

void ConnectionService::PostNotice(const std::string& receiver_uid, const Json::Value& notice)
{
	std::lock_guard lock(_mutex);

	auto it = _conn_to_id_map.find(receiver_uid);
	if (it != _conn_to_id_map.end())
	{
		if (it->second && it->second->connected())
			Utils::SendJson(it->second, notice);
		else
			_conn_to_id_map.erase(it);
	}
	
}

void ConnectionService::Broadcast(const std::vector<std::string>& targets, const Json::Value& message)
{
	if (targets.empty())
	{
		LOG_INFO << "not target to send";
		return;
	}

	int sent_count = 0;
	for (const auto& uid : targets)
	{
		if (SendIfConnected(uid, message))
			sent_count++;
	}
	LOG_INFO << "Broadcast complete: " << sent_count << "/" << targets.size() << " messages sent";
}

