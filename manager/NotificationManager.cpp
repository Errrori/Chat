#include "pch.h"
#include "NotificationManager.h"

NotificationManager& NotificationManager::GetInstance()
{
	static NotificationManager notification_manager(ConnectionManager::GetInstance());
	return notification_manager;
}

NotificationManager::NotificationManager(ConnectionManager& connection_manager)
	:_connection_manager(connection_manager),_is_processing(false)
{
}

void NotificationManager::CreateNotification(const Json::Value& msg)
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
		Json::Value notice;
		{
			std::lock_guard lock(_notice_mtx);
			if (_notice_queue.empty())
			{
				_is_processing = false;
				return;
			}
			notice = _notice_queue.front();
			_notice_queue.pop();
		}
		//先要写入关系表（可能有记录已经存在的情况）
		auto notice_type = Utils::Notice::StringToType(notice["action_type"].asString());
		std::string receiver_uid;
		switch (notice_type)
		{
		case Utils::Notice::ActionType::BlockUser:
			continue;
		case Utils::Notice::ActionType::UnblockUser:
			continue;
		case Utils::Notice::ActionType::FriendRequest:
			DatabaseManager::WriteFriendRequest(notice);
			receiver_uid = notice["receiver_uid"].asString();
			break;
		case Utils::Notice::ActionType::GroupRequest:
			//通过目标id也就是群聊id找到群组管理员/群主,向他们发送消息
			break;
		case Utils::Notice::ActionType::FriendResponse:
			receiver_uid = notice["receiver_uid"].asString();
			break;
		case Utils::Notice::ActionType::GroupResponse:
			//通过目标id也就是群聊id找到群组管理员/群主,向他们发送消息
			break;
		case Utils::Notice::ActionType::Unfriend:
			continue;
		default:
			LOG_ERROR << "Unknown action type";
			continue;
		}
		//查看目标用户是否在线,不考虑群聊

		auto conn = _connection_manager.GetConnection(notice["receiver_uid"].asString());
		if (!conn)
		{
		//目标用户不在线，把需要写入通知表的写入(屏蔽用户或者解除屏蔽，解除好友关系不需要通知)
			DatabaseManager::PushChatRecords(notice);
			continue;
		}
		//目标用户在线，向该用户发送通知
		notice["chat_type"] = "System";
		conn->sendJson(notice);
	}
}
