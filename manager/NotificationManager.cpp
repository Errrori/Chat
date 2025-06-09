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
	{
		std::lock_guard lock(_notice_mtx);
		_notice_queue.emplace(msg);
		if (_is_processing)
		{
			return;
		}
		_is_processing = true;
	}
	ProcessNotification();
}

void NotificationManager::ProcessNotification()
{
	drogon::app().getIOLoop(drogon::app().getCurrentThreadIndex())
		->queueInLoop([this]()
			{
			try
			{
				unsigned short message_send = 0;
				while (message_send < MAX_MESSAGE_SEND_ONCE)
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
					auto conn = _connection_manager.GetConnection(dto.GetReceiverUid());
					if (!conn)
					{
						//目标用户不在线，把需要写入通知表的写入
						DatabaseManager::WriteNotification(dto);
					}
					else
					{
						//目标用户在线，向该用户发送通知
						conn.value()->sendJson(dto.TransToJsonMsg());
					}
				}
				drogon::app().getIOLoop(drogon::app().getCurrentThreadIndex())
					->runAfter(MESSAGE_SEND_WAIT_TIME, [this]()
						{
							ProcessNotification();
						});
			}
			catch (const std::exception& e)
			{
				LOG_ERROR << "exception process notifications: " << e.what();
				_is_processing = false;
			}
			});
}
