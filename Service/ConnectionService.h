#pragma once
#include <unordered_map>
#include <drogon/WebSocketConnection.h>
#include "Common/ConnectionContext.h"
#include "Common/OutboundMessage.h"
class RedisService;

class ConnectionService
{
public:

	ConnectionService(const ConnectionService& manager) = delete;
	ConnectionService& operator=(const ConnectionService& manager) = delete;
	ConnectionService(ConnectionService&& manager) = delete;
	ConnectionService& operator=(ConnectionService&& manager) = delete;

	explicit ConnectionService(std::shared_ptr<RedisService> redis)
		: _redis_service(std::move(redis)) {}

	drogon::Task<bool> IsOnline(const std::string& uid);

	bool AddConnection(const drogon::WebSocketConnectionPtr& conn, const std::string& uid);
	bool RemoveConnection(const std::string& uid);
	bool RemoveConnection(const drogon::WebSocketConnectionPtr& conn);
	drogon::Task<ChatDelivery::DeliveryResult> DeliverToUser(
		const std::string& uid,
		const ChatDelivery::OutboundMessage& message);

	std::shared_ptr<ConnectionContext> GetConnInfo(const drogon::WebSocketConnectionPtr& conn) const;
	// Legacy wrappers
	bool SendIfConnected(const std::string& uid, const Json::Value& message);
	void PostNotice(const std::string& receiver_uid, const Json::Value& notice);
	void Broadcast(const std::vector<std::string>& targets, const Json::Value& message);

	~ConnectionService() = default;

private:
	bool SendEnvelopeOnline(const std::string& uid, const Json::Value& envelope);

	//id to connection
	std::unordered_map<std::string, drogon::WebSocketConnectionPtr> _conn_to_id_map;
	std::recursive_mutex _mutex;
	std::shared_ptr<RedisService> _redis_service;

	drogon::Task<> OnUserConnected(std::string uid);
	drogon::Task<> OnUserDisconnected(std::string uid);
};

