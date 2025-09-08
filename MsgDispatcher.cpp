#include "pch.h"
#include "MsgDispatcher.h"
#include "manager/ConnectionManager.h"
#include "manager/ThreadManager.h"

bool MsgDispatcher::DeliverMessage(const std::string& sender_uid,int thread_id, const Json::Value& msg)
{
	const auto& trans_msg = MessageManager::MsgData::BuildFromJson(msg);
	if (!trans_msg)
	{
		LOG_ERROR << "fail to build message data from json";
		return false;
	}
	const auto& msg_data = trans_msg.value();

	if (!MessageManager::ValidateMsg(msg_data))
	{
		LOG_ERROR << "message is not valid";
		return false;
	}

	//1.get receivers uid
	const auto& thread_members = ThreadManager::GetThreadMembersUid(thread_id);
	if (!thread_members.has_value())
	{
		LOG_ERROR << "can not get thread members uid, thread id: " << thread_id;
		return false;
	}

	const auto& connections = ConnectionManager::GetInstance().GetConnections(thread_members.value());

	for (const auto& conn:connections)
	{
		conn->sendJson(msg);
	}


	DatabaseManager::PushMessage(msg_data);
	return true;
}
