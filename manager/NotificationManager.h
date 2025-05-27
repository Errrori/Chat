#pragma once
#include <json/json.h>
#include <queue>
#include "ConnectionManager.h"

class NotificationManager
{
public:
	NotificationManager(NotificationManager&) = delete;
	NotificationManager(NotificationManager&&) = delete;
	static NotificationManager& GetInstance();
	void CreateNotification(const Json::Value& msg);
	void ProcessNotification();

private:
	NotificationManager(ConnectionManager& connection_manager);
private:
	std::queue<Json::Value> _notice_queue;
	std::mutex _notice_mtx;
	ConnectionManager& _connection_manager;
	bool _is_processing;
};

