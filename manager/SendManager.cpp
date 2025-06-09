#include "pch.h"
#include "SendManager.h"
#include "models/ChatRecords.h"


using namespace Utils;

SendManager& SendManager::GetInstance()
{
	static SendManager send_manager;
	return send_manager;
}

void SendManager::PushMessage(const MessageDTO& dto)
{
	{
		std::lock_guard lock(_msg_mtx);
		_msg_queue.emplace(dto);
		if (_is_processing)
		{
			return;
		}
		_is_processing = true;
	}
	ProcessMessage();
}

void SendManager::ProcessMessage()
{
	drogon::app().getIOLoop(drogon::app().getCurrentThreadIndex())->
	queueInLoop([this]()
	{
		try
		{
			unsigned short message_send = 0;
			while (message_send < MAX_MESSAGE_SEND_ONCE)
			{
				MessageDTO dto;
				{
					std::lock_guard lock(_msg_mtx);
					if (_msg_queue.empty()) {
						_is_processing = false;
						return;
					}
					dto = _msg_queue.front();
					_msg_queue.pop();
				}

				std::string error;
				if (!ValidateMsg(dto, error))
				{
					LOG_INFO << "error to validate message: " << error;

					continue;
				}
				DeliverMessage(dto);
				message_send++;
			}
			drogon::app().getIOLoop(drogon::app().getCurrentThreadIndex())
				->runAfter(MESSAGE_SEND_WAIT_TIME, [this]()
					{
						ProcessMessage();
					});
		}
		catch (const std::exception& e)
		{
			_is_processing = false;
			LOG_ERROR << "exception process message: " << e.what();
		}
	});
}


bool SendManager::ValidateMsg(const MessageDTO& dto, std::string& error)
{
	if (!DatabaseManager::ValidateUid(dto.GetSenderUid()))
	{
		error = "sender_uid is not valid";
		return false;
	}
	switch (dto.GetChatType())
	{
	case ChatType::Private:
		if (!DatabaseManager::ValidateUid(dto.GetReceiverUid().value()))
		{
			error = "receiver_uid is not valid";
			return false;
		}
		break;
	case ChatType::Group:
		//validate group id
		break;
	default:
		break;
	}
	return true;
}

void SendManager::DeliverMessage(const MessageDTO& message_dto)
{
	auto conn = ConnectionManager::GetInstance().GetConnection(message_dto.GetReceiverUid().value());
	auto chat_record = message_dto.TransToChatRecords();
	DatabaseManager::PushChatRecords(chat_record.value());
	if (!chat_record.has_value())
	{
		LOG_ERROR << "can not trans to chat record";
		return;
	}

	if (!conn.has_value())
	{
		LOG_INFO << "can not find conn";
		return;
	}
	conn.value()->sendJson(message_dto.TransToJsonMsg());
	//∑¢ÀÕ»∑»œ
	
}