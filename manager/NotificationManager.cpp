#include "pch.h"
#include "NotificationManager.h"
#include "DTOs/NotificationDTO.h"

NotificationManager& NotificationManager::GetInstance()
{
	static NotificationManager notification_manager(ConnectionManager::GetInstance());
	return notification_manager;
}

NotificationManager::NotificationManager(ConnectionManager& connection_manager)
	:_connection_manager(connection_manager),_is_processing(false) {}

void NotificationManager::PushNotification(const NotificationDTO& msg)
{
	//check if target user is online,
	//However, regardless of whether the user is online or not, the relationship table should be written
	std::lock_guard lock(_notice_mtx);
	_notice_queue.emplace(msg);
	if (_is_processing)
	{
		return;
	}
	ProcessNotification();
}

void NotificationManager::ProcessNotification()
{
	while (true)
	{
		NotificationDTO dto;
		{
			std::lock_guard lock(_notice_mtx);
			if (_notice_queue.empty())
			{
				_is_processing = false;
				return;
			}
			dto = _notice_queue.front();
			_notice_queue.pop();
		}
		
		//查看目标用户是否在线,不考虑群聊

		auto conn = _connection_manager.GetConnection(dto.GetReceiverUid());
		if (!conn)
		{
			//目标用户不在线，把需要写入通知表的写入
			DatabaseManager::WriteNotification(dto);
			continue;
		}

		//目标用户在线，向该用户发送通知
		conn->sendJson(dto.TransToJsonMsg());
	}
}
