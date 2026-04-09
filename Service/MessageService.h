#pragma once
#include "Common/AIMessage.h"
#include "Data/IMessageRepository.h"


class RedisService;
class RelationshipService;

namespace drogon_model::postgres
{
	class Messages;
}
class ChatThread;
class AIMessage;
class ConnectionService;
class ThreadService;
class IMsgProcessor;
class RequestMsg;

struct MsgProcessedResult
{
	bool success;
	std::string error;
	bool partial_degraded{false};
	int redis_failed_targets{0};
};

class MessageService
{
public:
	typedef std::function<void(const Json::Value&)> ErrorCb;

	MessageService(std::shared_ptr<IMessageRepository> repo,
	               std::shared_ptr<ConnectionService> conn,
	               std::shared_ptr<ThreadService> thread_service,
	               std::shared_ptr<RedisService> redis)
		:_msg_repo(std::move(repo)), _conn_service(std::move(conn)),
		 _thread_service(std::move(thread_service)), _redis_service(std::move(redis)){}

	void SetRelationshipService(std::shared_ptr<RelationshipService> rel) { _relationship_service = std::move(rel); }
	//,_thread_service(std::move(thread_service)))
	drogon::Task<Json::Value> GetChatRecords(int thread_id, int num, int64_t existed_id = 0);
	drogon::Task<Json::Value> GetAIRecords(int thread_id, int64_t existed_time = 0);

	void ProcessUserMsg(ChatMessage msg, const ErrorCb& cb) const;

	drogon::Task<MsgProcessedResult> ProcessChatMsg(int thread_id, const std::string& sender_uid,
	                                                std::optional<std::string> content,
	                                                std::optional<Json::Value> attachment) const;

	void ProcessAIRequest(Json::Value msg, drogon::WebSocketConnectionPtr conn) const;



	drogon::Task<Json::Value> GetChatOverviews(int64_t existing_id, const std::string& uid) const;

private:
	std::shared_ptr<IMessageRepository> _msg_repo;
	std::shared_ptr<ConnectionService> _conn_service;
	std::shared_ptr<ThreadService> _thread_service;
	std::shared_ptr<RedisService> _redis_service;
	std::shared_ptr<RelationshipService> _relationship_service;
};

