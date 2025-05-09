#include "ConnectionManager.h"
#include "Users.h"

ConnectionManager& ConnectionManager::GetInstance()
{
	static ConnectionManager manager;
	return manager;
}

bool ConnectionManager::AddConnection(const Utils::UserInfo& info, const drogon::WebSocketConnectionPtr& conn)
{
	auto uid = info.uid;
	std::unique_lock lock(_conn_mtx);
	auto it = _conn_map.find(uid);
	if (it != _conn_map.end())
	{
		if (it->second == conn)
		{
			LOG_INFO << "Insert the same value repeatedly";
			return true;
		}
		LOG_ERROR << "The key already had a value,and the new value is not the same as the existed one";
		return false;
	}

	conn->setContext(std::make_shared<Utils::UserInfo>(info));
	_conn_map.emplace(uid, conn);
	lock.unlock();

	Json::Value data;
	if (Users::GetUserInfoByUid(uid,data))
	{
		auto name = data["username"].asString();
		return AddUIdToNameRef(uid, name);
	}
	
	LOG_ERROR << "can not add UID to name reference, since can not get user info";
	return false;
}

bool ConnectionManager::RemoveConnection(const std::string& uid)
{
	std::lock_guard lock(_conn_mtx);
	auto it = _conn_map.find(uid);
	if (it == _conn_map.end())
	{
		LOG_ERROR << "Can not find connection,uid :" << uid;
		return false;
	}

	_conn_map.erase(uid);
	return true;
}

bool ConnectionManager::RemoveConnection(const drogon::WebSocketConnectionPtr& conn)
{
	auto info_ptr = conn->getContext<Utils::UserInfo>();
	if (info_ptr)
	{
		return RemoveConnection(info_ptr->uid);
	}
	LOG_ERROR << "Error to get ConnectionPtr context";
	return false;
}

bool ConnectionManager::AddUIdToNameRef(const std::string& uid, const std::string& name)
{
	std::lock_guard lock(_name_mtx);
	auto it = _name_map.find(uid);
	if (it != _name_map.end())
	{
		if (it->second == name)
		{
			LOG_INFO << "Insert the same value repeatedly";
			return true;
		}
		LOG_ERROR << "The key already had a value,and the new value is not the same as the existed one";
		return false;
	}
	_name_map[uid] = name;
	return true;
}

void ConnectionManager::BroadcastMsg(const std::string& uid, const Json::Value& msg) const
{
	LOG_INFO << "access broadcast message :"<<msg.toStyledString();
	std::cout << "access broadcast message :"<<msg.toStyledString();
	for (const auto& it : _conn_map)
	{
		if (it.first!=uid)
		{
			LOG_INFO << "send message to" << GetInstance().GetName(it.first);
			it.second->sendJson(msg);
		}
	}
}

void ConnectionManager::BroadcastMsg(const std::string& uid, const std::string& msg) const
{
	LOG_INFO <<GetInstance().GetName(uid)<< " access broadcast message :" << msg;
	for (const auto& it : _conn_map)
	{
		if (it.first != uid)
		{
			LOG_INFO << "send message to" << GetInstance().GetName(it.first);
			it.second->send(msg);
		}
	}
}

std::string ConnectionManager::GetName(const std::string& uid)
{
	std::lock_guard lock(_name_mtx);
	auto it = _name_map.find(uid);
	if (it==_name_map.end())
	{
		LOG_INFO << "can not find name, uid:" << uid;
		return {};
	}
	return it->second;
}

drogon::WebSocketConnectionPtr ConnectionManager::GetConnection(const std::string& uid)
{
	std::lock_guard lock(_conn_mtx);
	auto it = _conn_map.find(uid);
	if (it == _conn_map.end())
	{
		LOG_INFO << "can not find connection, uid:" << uid;
		return nullptr;
	}
	return it->second;
}
