#pragma once
#include <unordered_map>
#include <drogon/WebSocketConnection.h>
#include <drogon/utils/coroutine.h>
#include "Common/ConnectionContext.h"
#include "Common/OutboundMessage.h"
class RedisService;

class ConnectionService:public std::enable_shared_from_this<ConnectionService>
{
public:
	ConnectionService(const ConnectionService& manager) = delete;
	ConnectionService& operator=(const ConnectionService& manager) = delete;
	ConnectionService(ConnectionService&& manager) = delete;
	ConnectionService& operator=(ConnectionService&& manager) = delete;
	explicit ConnectionService(std::shared_ptr<RedisService> redis)
		: _redis_service(std::move(redis)) {}

	drogon::Task<bool> IsOnline(const std::string& uid);
	bool AddConnection(const drogon::WebSocketConnectionPtr& conn, const std::string& uid,
		std::chrono::time_point<std::chrono::system_clock> expiry);
	bool RemoveConnection(const drogon::WebSocketConnectionPtr& conn);
	bool RemoveConnection(const std::string& uid);
	drogon::Task<ChatDelivery::DeliveryResult> DeliverToUser(
		const std::string& uid,
		const ChatDelivery::OutboundMessage& message);

	std::shared_ptr<ConnectionContext> GetConnInfo(const drogon::WebSocketConnectionPtr& conn) const;
	void RemoveUserConn(const std::string& uid);

	~ConnectionService() = default;

private:
	bool SendEnvelopeOnline(const std::string& uid, const Json::Value& envelope);

	//id to connection
	std::unordered_map<std::string, drogon::WebSocketConnectionPtr> _conn_to_id_map;
	std::recursive_mutex _mutex;
	std::shared_ptr<RedisService> _redis_service;

	drogon::Task<> OnUserConnected(std::string uid);
	drogon::Task<> OnUserDisconnected(std::string uid, trantor::TimerId timer_id);
};

