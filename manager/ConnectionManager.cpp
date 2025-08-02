#include "pch.h"
#include "ConnectionManager.h"


ConnectionManager& ConnectionManager::GetInstance()
{
	static ConnectionManager manager;
	return manager;
}

bool ConnectionManager::AddConnection(const Utils::UserInfo& info, const drogon::WebSocketConnectionPtr& conn)
{
	auto uid = info.uid;
	{
		std::lock_guard lock(_conn_mtx);
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
	}

	Json::Value user_data;
	if (!DatabaseManager::GetUserInfoByUid(uid, user_data))
	{
		return false;
	}
	Utils::UserInfo conn_info;
	conn_info.uid = user_data["uid"].asString();
	conn_info.account = user_data["account"].asString();
	conn_info.username = user_data["username"].asString();
	conn_info.avatar = user_data["avatar"].asString();
	LOG_INFO << "user info:" << conn_info.ToString();
	conn->setContext(std::make_shared<Utils::UserInfo>(conn_info));

	std::lock_guard lock(_conn_mtx);

	_conn_map.emplace(uid, conn);

	LOG_INFO << "Add Connection: " << uid;

	return AddUIdToNameRef(uid, info.username);
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

	LOG_INFO << "Remove Connection: " << uid;
	return true;
}

bool ConnectionManager::RemoveConnection(const drogon::WebSocketConnectionPtr& conn)
{
	auto info_ptr = conn->getContext<Utils::UserInfo>();
	if (info_ptr)
	{
		LOG_INFO << "Remove Connection: " << info_ptr->uid;
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

void ConnectionManager::BroadcastMsg(const std::string& uid, const Json::Value& msg)
{
	LOG_INFO << "broadcast message\n";
	std::lock_guard lock(_conn_mtx);
	if (msg.empty())
	{
		return;
	}
	DatabaseManager::PushMessage(TransMsg::BuildFromJson(msg));
	LOG_INFO << "push new message into database:"+msg.toStyledString();
	for (const auto& it : _conn_map)
	{
		if (it.first!=uid)
		{
			it.second->sendJson(msg);
		}
	}
}

void ConnectionManager::BroadcastMsg(const std::string& uid, const std::string& msg)
{
	std::lock_guard lock(_conn_mtx);
	for (const auto& it : _conn_map)
	{
		if (it.first != uid)
		{
			it.second->send(msg);
		}
	}
}

Json::Value ConnectionManager::GetOnlineUsers()
{
	std::lock_guard c_lock(_conn_mtx);
	std::lock_guard n_lock(_name_mtx);
	Json::Value data(Json::arrayValue);
	for (const auto& it: _conn_map)
	{
		auto& uid = it.first;
		Json::Value userInfo;
		if (DatabaseManager::GetUserInfoByUid(uid, userInfo)) {
			Json::Value user;
			user["uid"] = uid;
			user["username"] = _name_map.at(uid);
			user["avatar"] = userInfo["avatar"].asString();
			data.append(user);
		} else {
			Json::Value user;
			user["uid"] = uid;
			user["username"] = _name_map.at(uid);
			user["avatar"] = "https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png";
			data.append(user);
		}
	}
	return data;
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

std::optional<drogon::WebSocketConnectionPtr> ConnectionManager::GetConnection(const std::string& uid)
{
	std::lock_guard lock(_conn_mtx);
	auto it = _conn_map.find(uid);
	if (it == _conn_map.end())
	{
		LOG_INFO << "can not find connection, uid:" << uid;
		return std::nullopt;
	}
	return it->second;
}

std::vector<drogon::WebSocketConnectionPtr> ConnectionManager::GetConnection(const std::vector<std::string>& uids)
{
	return {};
}
