#pragma once
#include <optional>
#include <unordered_map>
#include <drogon/WebSocketConnection.h>
#include <drogon/utils/coroutine.h>
#include "Common/ConnectionContext.h"
#include "Common/OutboundMessage.h"
class RedisService;
class ClusterService;

struct ConnectionStateSnapshot
{
	std::string uid;
	std::chrono::time_point<std::chrono::system_clock> expiry;
	bool is_delivering_offline{false};
};

class ConnectionService:public std::enable_shared_from_this<ConnectionService>
{
public:
	ConnectionService(const ConnectionService& manager) = delete;
	ConnectionService& operator=(const ConnectionService& manager) = delete;
	ConnectionService(ConnectionService&& manager) = delete;
	ConnectionService& operator=(ConnectionService&& manager) = delete;
	explicit ConnectionService(std::shared_ptr<RedisService> redis)
		: _redis_service(std::move(redis)) {}

	/// 注入集群通信层；为空则退化为单机模式
	void SetClusterService(std::shared_ptr<ClusterService> cluster)
	{
		_cluster_service = std::move(cluster);
	}

	drogon::Task<bool> IsOnline(const std::string& uid);
	bool AddConnection(const drogon::WebSocketConnectionPtr& conn, const std::string& uid,
		std::chrono::time_point<std::chrono::system_clock> expiry);
	bool RemoveConnection(const drogon::WebSocketConnectionPtr& conn);
	bool RemoveConnection(const std::string& uid);

	/// 投递消息：本地在线直发 → 跨节点路由 → 离线队列
	drogon::Task<ChatDelivery::DeliveryResult> DeliverToUser(
		const std::string& uid,
		const ChatDelivery::OutboundMessage& message);

	/// 仅本地投递：本地在线直发，否则离线入队（不做跨节点路由）。
	/// 供 ClusterService 收到其他节点转发来的消息时调用。
	drogon::Task<ChatDelivery::DeliveryResult> DeliverLocal(
		const std::string& uid,
		const ChatDelivery::OutboundMessage& message);

	std::shared_ptr<ConnectionContext> GetConnInfo(const drogon::WebSocketConnectionPtr& conn) const;
	std::optional<ConnectionStateSnapshot> GetConnectionSnapshot(
		const drogon::WebSocketConnectionPtr& conn) const;
	void TouchConnection(const drogon::WebSocketConnectionPtr& conn) const;
	void RemoveUserConn(const std::string& uid);

	/// 跨节点强踢：仅关闭本机该用户的连接，不改变在线状态、路由与离线队列。
	/// 供 ClusterService 收到其他节点的 kick 控制消息时调用。
	void KickLocalSession(const std::string& uid, const std::string& reason);

	/// 本节点当前持有的本地连接数（可观测性）
	size_t LocalConnectionCount();

	/// 优雅退出：CAS 清空本节点所有在线用户的 route:user，幂等副作用交给对端。
	void ClearLocalRoutes();

	/// Refresh the connection's access token via WebSocket, update expiry and reset disconnect timer
	bool RefreshConnectionToken(const drogon::WebSocketConnectionPtr& conn,
		const std::string& new_access_token);

	/// Start heartbeat monitor timer (call after drogon event loop is ready)
	void StartHeartbeatMonitor();

	~ConnectionService() = default;

private:
	bool SendEnvelopeOnline(const std::string& uid, const Json::Value& envelope);

	/// 将消息写入离线队列（不检查本地连接）
	drogon::Task<ChatDelivery::DeliveryResult> QueueOffline(
		const std::string& uid,
		const ChatDelivery::OutboundMessage& message,
		const Json::Value& envelope);

	/// Reset the connection's disconnect timer
	bool ResetExpiryTimer(const drogon::WebSocketConnectionPtr& conn,
		std::shared_ptr<ConnectionContext>& ctx,
		std::chrono::time_point<std::chrono::system_clock> new_expiry);

	/// Periodically scan all connections, clean zombies & send expiry warnings
	void RunHeartbeatCheck();

	//id to connection
	std::unordered_map<std::string, drogon::WebSocketConnectionPtr> _conn_to_id_map;
	std::mutex _mutex;
	std::shared_ptr<RedisService> _redis_service;
	std::shared_ptr<ClusterService> _cluster_service;
	trantor::TimerId _monitor_timer_id{};

	drogon::Task<> OnUserConnected(std::string uid);
	drogon::Task<> OnUserDisconnected(std::string uid, trantor::TimerId timer_id);
};

