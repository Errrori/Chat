#pragma once
#include <queue>
#include "ConnectionManager.h"

using SendPair = std::pair<std::vector<std::string>, Json::Value>;
using MsgPair = std::pair<drogon::WebSocketConnectionPtr, Json::Value>;

class SendManager
{
public:
	void PushMessage(const Json::Value& msg, const drogon::WebSocketConnectionPtr& conn);
	static SendManager& GetInstance();

private:
	SendManager() = default;

	void ProcessMessage();
	bool ValidateMsg(const MsgPair& msg_pair, std::string& error);
	SendPair BuildDeliverMsg(const MsgPair& msg_pair);
	void DeliverMessage(const SendPair& send_pair);

private:
	std::queue<MsgPair> _msg_queue;
	std::mutex _msg_mtx;
	bool _is_processing = false;
};



