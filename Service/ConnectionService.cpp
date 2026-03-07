#include "pch.h"
#include "ConnectionService.h"
#include "RedisService.h"
#include "Common/User.h"

// 查询用户是否在线（先查本进程内存，再查 Redis 以支持跨节点）
drogon::Task<bool> ConnectionService::IsOnline(const std::string& uid)
{
	{
		std::lock_guard lock(_mutex);
		if (_conn_to_id_map.count(uid))
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

	// 异步标记在线状态（不阻塞）
	const auto uid = conn->getContext<UserInfo>()->getUid();
	drogon::async_run([this, uid]() -> drogon::Task<>
	{
		co_await _redis_service->SetOnline(uid);
		// 投递离线消息（在 MessageService 打包后随即可用）
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
	}());

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

	// 异步清除 Redis 在线状态
	drogon::async_run([this, uid]() -> drogon::Task<>
	{
		co_await _redis_service->SetOffline(uid);
	}());

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

