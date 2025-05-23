#include "pch.h"
#include "SendManager.h"

using namespace Utils;

SendManager& SendManager::GetInstance()
{
	static SendManager send_manager;
	return send_manager;
}

void SendManager::PushMessage(const Json::Value& msg, const drogon::WebSocketConnectionPtr& conn)
{
	std::lock_guard lock(_msg_mtx);
	_msg_queue.emplace(conn, msg);
	if (_is_processing)
	{
		return;
	}
	ProcessMessage();
	_is_processing = true;
}

void SendManager::ProcessMessage()
{
	drogon::app().getIOLoop(drogon::app().getCurrentThreadIndex())->
	queueInLoop([this]()
	{
		while (true)
		{
			{
				std::lock_guard lock(_msg_mtx);
				if (_msg_queue.empty()) {
					_is_processing = false;
					return;
				}
			}
			auto msg_pair = _msg_queue.front();
			_msg_queue.pop();
			std::string error;
			if (!ValidateMsg(msg_pair, error))
			{
				LOG_INFO << "error to validate message: "<<error;
				msg_pair.first->sendJson(Message::GenerateErrorMsg(error));
				continue;
			}
			auto send_pair = BuildDeliverMsg(msg_pair);
			DeliverMessage(send_pair);
		}
	});
}


bool SendManager::ValidateMsg(const MsgPair& msg_pair, std::string& error)
{
	//������Ҫ���鷢�͵�Ŀ�������Ƿ�������ݿ�
	const auto& msg = msg_pair.second;
	const auto& conn = msg_pair.first;
	//Ҫ����Ҫ�ֶ��Ƿ���ڣ������ֶ��Ƿ�Ϸ�
	Json::Value deliver_msg;
	if (!msg.isMember("content_type") || !msg.isMember("content"))
	{
		error = "lack of necessary field";
		return false;
	}

	//��֤uid�Ƿ����
	if (msg.isMember("receiver_uid"))
	{
		if (!DatabaseManager::ValidateUid(msg["receiver_uid"].asString()))
		{
			error = "user uid is not correct";
			return false;
		}
	}

	auto content_type = msg["content_type"].asString();
	if (!Message::Content::IsValid(content_type))
	{
		error = "content_type is not supported: "+content_type;
		return false;
	}

	//�������conversation_id������chat_type��֤������Ⱥ�ģ���֤Ⱥ��uid�Ƿ���ڣ��������Ƿ�ΪȺ��Ա������ʱ����֤
	auto chat_type = msg["chat_type"].asString();
	if (!Message::Chat::IsValid(chat_type))
	{
		error = "chat Type is not supported: " + chat_type;
		return false;
	}
	//�������˽�ģ�����Ƿ��ж�Ӧ��Ҫ��id,Ŀǰֻ��Ҫ����Ⱥ�ģ�����id������չ
	switch (Message::Chat::StringToType(chat_type))
	{
	case Message::Chat::ChatType::Group:
		if (!msg.isMember("conversation_id"))
		{
			error = "lack of conversation id";
			return false;
		}
		break;
	default:
		break;
	}
	return true;
}

SendPair SendManager::BuildDeliverMsg(const MsgPair& msg_pair)
{
	//������Ϣ��������Ŀ������
	const auto& sender_conn = msg_pair.first;
	const auto& msg = msg_pair.second;
	Json::Value deliver_msg;
	auto sender_info = *(sender_conn->getContext<Connection::UserConnectionInfo>());
	//�ȹ���������Ϣ��Ҫ����
	deliver_msg["message_id"] = Message::GenerateMsgId();
	deliver_msg["create_time"] = trantor::Date::now().toDbString();
	deliver_msg["sender_uid"] = sender_info.uid;
	deliver_msg["sender_avatar"] = sender_info.avatar;
	deliver_msg["username"] = sender_info.username;
	deliver_msg["content"] = msg["content"].asString();
	deliver_msg["content_type"] = msg["content_type"].asString();
	deliver_msg["chat_type"] = msg["chat_type"].asString();
	std::vector<std::string> targets;

	// ��Ⱥ��/˽�Ĵ���,�Ȳ�����@����
	if (msg.isMember("conversation_id"))
	{
		//����û�м��conversation_name,Ҳû�м��������Ƿ�ΪȺ��Ա
		deliver_msg["conversation_id"] = msg["conversation_id"].asString();
		deliver_msg["conversation_name"] = msg["conversation_name"].asString();
		//��Ⱥ�ı��в���Ⱥ��Ա,Ȼ�����
		//auto group_member = DatabaseManager::GetGroupMember();
		//targets.emplace_back(group_member);

	}
	if (msg.isMember("receiver_uid"))
	{
		deliver_msg["receiver_uid"] = msg["receiver_uid"].asString();
		targets.emplace_back(msg["receiver_uid"].asString());
	}
	//... �����Ĵ����߼�

	//д�����ݿ�
	DatabaseManager::PushChatRecords(deliver_msg);

	//���췢����������гɷ�
	return std::make_pair(targets, deliver_msg);
}

void SendManager::DeliverMessage(const SendPair& send_pair)
{
	const auto& targets = send_pair.first;
	auto msg = send_pair.second;
	for (const auto& target:targets)
	{
		const auto& conn = ConnectionManager::GetInstance().GetConnection(target);
		if (conn!=nullptr)
		{
			conn->sendJson(msg);
		}
	}
}