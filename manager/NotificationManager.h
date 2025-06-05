#pragma once
#include <queue>
#include "ConnectionManager.h"
#include "DTOs/NotificationDTO.h"

class NotificationManager
{
public:
	NotificationManager(const NotificationManager&) = delete;
	NotificationManager& operator=(const NotificationManager&) = delete;
	~NotificationManager() = default;

	static NotificationManager& GetInstance();
	void PushNotification(const NotificationDTO& msg);
	void ProcessNotification();

private:
	NotificationManager(ConnectionManager& connection_manager);

private:
	std::queue<NotificationDTO> _notice_queue;
	std::mutex _notice_mtx;
	ConnectionManager& _connection_manager;
	bool _is_processing;
};

