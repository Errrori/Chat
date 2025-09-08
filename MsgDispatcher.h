#pragma once
class MsgDispatcher
{
public:
	static bool DeliverMessage(const std::string& sender_uid,int thread_id, const Json::Value& msg);

};

