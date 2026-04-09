#include "pch.h"
#include "ConnectionService.h"
#include "RedisService.h"
#include "Common/HeartbeatConfig.h"
#include "auth/TokenService.h"
#include <drogon/utils/coroutine.h>

namespace
{
constexpr char kDeliveryReasonInvalidEnvelope[] = "invalid envelope";
constexpr char kDeliveryReasonSent[] = "sent";
constexpr char kDeliveryReasonOfflineDropped[] = "offline dropped";
constexpr char kDeliveryReasonQueued[] = "queued";
constexpr char kDeliveryReasonQueuedRedisFailed[] = "queued(redis_failed,db_only)";
}

bool ConnectionService::SendEnvelopeOnline(const std::string& uid, const Json::Value& envelope)
{
	drogon::WebSocketConnectionPtr conn;
	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		if (it == _conn_to_id_map.end())
			return false;

		if (!(it->second && it->second->connected()))
		{
			_conn_to_id_map.erase(it);
			return false;
		}

		conn = it->second;
	}

	Utils::SendJson(conn, envelope);
	return true;
}

drogon::Task<bool> ConnectionService::IsOnline(const std::string& uid)
{
	{
		std::lock_guard lock(_mutex);
		if (_conn_to_id_map.find(uid) != _conn_to_id_map.end())
			co_return true;
	}
	co_return co_await _redis_service->IsOnline(uid);
}

bool ConnectionService::AddConnection(const drogon::WebSocketConnectionPtr& conn, const std::string& uid, 
	std::chrono::time_point<std::chrono::system_clock> expiry)
{
	try
	{
		drogon::WebSocketConnectionPtr old_conn;
		{
			std::lock_guard lock(_mutex);
			auto it = _conn_to_id_map.find(uid);
			if (it != _conn_to_id_map.end())
			{
				LOG_INFO << "user already have a connection";
				old_conn = it->second;
				_conn_to_id_map.erase(it);
			}
			_conn_to_id_map.emplace(uid, conn);
		}

		if (old_conn && old_conn->connected())
		{
			LOG_INFO << "kick the old connection";
			old_conn->shutdown(drogon::CloseCode::kViolation, "another device log in");
		}
			LOG_INFO << "new user connected: " << uid;

			auto remaining_time = static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>
				(expiry - std::chrono::system_clock::now()).count());

			if (remaining_time<=10)
			{
				conn->shutdown(drogon::CloseCode::kNormalClosure, "please refresh your token,it's expiry is near");
				return false;
			}

			auto delay = remaining_time + Utils::GetRandomJitter();

			trantor::TimerId timer_id;
			if (conn&& conn->connected())
			{
				timer_id = drogon::app().getLoop()->runAfter(delay, [weak_conn = std::weak_ptr(conn)]()
					{
						auto locked_conn = weak_conn.lock();
						if (locked_conn && locked_conn->connected())
						{
							locked_conn->shutdown(drogon::CloseCode::kNormalClosure,
								"access token expired");
						}
					});
			}
			else
			{
				return false;
			}

			auto ctx = std::make_shared<ConnectionContext>();
			ctx->uid = uid;
			ctx->timer_id = timer_id;
			ctx->expiry = expiry;
			ctx->last_active_time = std::chrono::system_clock::now();
			conn->setContext(std::move(ctx));

			// 启用 Drogon 传输层 ping/pong 保活
			conn->setPingMessage("",
				std::chrono::duration<double>(Heartbeat::PingIntervalSec));
		

		const auto connected_uid = conn->getContext<ConnectionContext>()->uid;
		drogon::async_run([self = shared_from_this(), connected_uid]() -> drogon::Task<> {
			co_await self->OnUserConnected(connected_uid);
			});

		return true;
	}catch (const std::exception& e)
	{
		LOG_ERROR << "exception: " << e.what();
		return false;
	}
}


drogon::Task<> ConnectionService::OnUserConnected(std::string uid)
{
	co_await _redis_service->SetOnline(uid);
	auto notices = co_await _redis_service->PopAllOfflineNotices(uid);
	drogon::WebSocketConnectionPtr conn;
	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		if (it == _conn_to_id_map.end() || !(it->second && it->second->connected()))
		{
			if (it != _conn_to_id_map.end())
				_conn_to_id_map.erase(it);
			co_return;
		}

		conn = it->second;
	}

	if (auto context = conn ? conn->getContext<ConnectionContext>() : nullptr)
	{
		std::lock_guard<std::mutex> state_lock(context->mutex);
		context->is_delivering_offline = true;
	}

	for (const auto& notice_str : notices)
	{
		if (!(conn && conn->connected()))
			break;
		conn->send(notice_str);
	}

	const auto moved = co_await _redis_service->MoveOfflineMessagesToPending(uid);
	if (moved)
	{
		while (conn && conn->connected())
		{
			auto pending_msg = co_await _redis_service->PopPendingOfflineMessage(uid);
			if (!pending_msg.has_value())
				break;

			if (!(conn && conn->connected()))
				break;

			conn->send(*pending_msg);
		}
	}

	if (auto context = conn ? conn->getContext<ConnectionContext>() : nullptr)
	{
		std::lock_guard<std::mutex> state_lock(context->mutex);
		context->is_delivering_offline = false;
	}
}

drogon::Task<ChatDelivery::DeliveryResult> ConnectionService::DeliverToUser(
	const std::string& uid,
	const ChatDelivery::OutboundMessage& message)
{
	ChatDelivery::DeliveryResult result;
	result.state = ChatDelivery::DeliveryState::Dropped;

	const auto envelope = message.ToEnvelope();
	if (envelope.isNull())
	{
		result.reason = kDeliveryReasonInvalidEnvelope;
		co_return result;
	}

	drogon::WebSocketConnectionPtr conn;
	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		if (it != _conn_to_id_map.end())
		{
			if (it->second && it->second->connected())
				conn = it->second;
			else
				_conn_to_id_map.erase(it);
		}
	}

	if (conn)
	{
		Utils::SendJson(conn, envelope);
		result.state = ChatDelivery::DeliveryState::Sent;
		result.reason = kDeliveryReasonSent;
		co_return result;
	}

	if (message.policy == ChatDelivery::DeliveryPolicy::OnlineOnly)
	{
		result.reason = kDeliveryReasonOfflineDropped;
		co_return result;
	}

	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	const auto serialized = Json::writeString(builder, envelope);
	const auto pushed = co_await _redis_service->PushOfflinePacket(uid, serialized, message.channel);

	result.state = ChatDelivery::DeliveryState::Queued;
	if (!pushed)
	{
		result.reason = kDeliveryReasonQueuedRedisFailed;
		result.degraded = true;
		result.redisQueueFailed = true;
		LOG_ERROR << "[Delivery] Redis queue failed for uid=" << uid
			<< ", message remains DB-only until fallback replay";
	}
	else
	{
		result.reason = kDeliveryReasonQueued;
	}
	co_return result;
}

drogon::Task<> ConnectionService::OnUserDisconnected(std::string uid, trantor::TimerId timer_id)
{
	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		if (it != _conn_to_id_map.end() && it->second && it->second->connected())
		{
			drogon::app().getLoop()->invalidateTimer(timer_id);
			co_return;
		}
	}

	drogon::app().getLoop()->invalidateTimer(timer_id);
	co_await _redis_service->SetOffline(uid);
	co_await _redis_service->RestorePendingOfflineMessages(uid);
}

bool ConnectionService::RemoveConnection(const drogon::WebSocketConnectionPtr& conn)
{
	auto context = conn ? conn->getContext<ConnectionContext>() : nullptr;
	if (!context)
	{
		LOG_ERROR << "Error to get ConnectionPtr context";
		return false;
	}

	const auto uid = context->uid;
	bool removed_current = false;

	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		if (it != _conn_to_id_map.end() && conn == it->second)
		{
			_conn_to_id_map.erase(it);
			removed_current = true;
		}
	}

	trantor::TimerId timer_id;
	{
		std::lock_guard<std::mutex> state_lock(context->mutex);
		timer_id = context->timer_id;
	}
	drogon::app().getLoop()->invalidateTimer(timer_id);
	if (!removed_current)
		return false;

	LOG_INFO << "Remove Connection: " << uid;
	drogon::async_run([self = shared_from_this(), uid, timer_id]() -> drogon::Task<> {
		co_await self->OnUserDisconnected(uid, timer_id);
	});
	return true;
}

bool ConnectionService::RemoveConnection(const std::string& uid)
{
	drogon::WebSocketConnectionPtr conn;
	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		if (it == _conn_to_id_map.end())
			return false;

		conn = it->second;
		_conn_to_id_map.erase(it);
	}

	trantor::TimerId timer_id;
	if (auto context = conn ? conn->getContext<ConnectionContext>() : nullptr)
	{
		std::lock_guard<std::mutex> state_lock(context->mutex);
		timer_id = context->timer_id;
	}

	drogon::app().getLoop()->invalidateTimer(timer_id);
	drogon::async_run([self = shared_from_this(), uid, timer_id]() -> drogon::Task<> {
		co_await self->OnUserDisconnected(uid, timer_id);
	});
	return true;
}

std::shared_ptr<ConnectionContext> ConnectionService::GetConnInfo(const drogon::WebSocketConnectionPtr& conn) const
{
	if (conn && conn->connected())
		return conn->getContext<ConnectionContext>();

	return nullptr;
}

std::optional<ConnectionStateSnapshot> ConnectionService::GetConnectionSnapshot(
	const drogon::WebSocketConnectionPtr& conn) const
{
	if (!(conn && conn->connected()))
		return std::nullopt;

	auto context = conn->getContext<ConnectionContext>();
	if (!context)
		return std::nullopt;

	std::lock_guard<std::mutex> state_lock(context->mutex);
	return ConnectionStateSnapshot{ context->uid, context->expiry, context->is_delivering_offline };
}

void ConnectionService::TouchConnection(const drogon::WebSocketConnectionPtr& conn) const
{
	if (!(conn && conn->connected()))
		return;

	auto context = conn->getContext<ConnectionContext>();
	if (!context)
		return;

	std::lock_guard<std::mutex> state_lock(context->mutex);
	context->last_active_time = std::chrono::system_clock::now();
}

void ConnectionService::RemoveUserConn(const std::string& uid)
{
	drogon::WebSocketConnectionPtr conn;
	trantor::TimerId timer_id;
	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		if (it == _conn_to_id_map.end())
			return;

		conn = it->second;
		_conn_to_id_map.erase(it);
	}

	if (auto context = conn ? conn->getContext<ConnectionContext>() : nullptr)
	{
		std::lock_guard<std::mutex> state_lock(context->mutex);
		timer_id = context->timer_id;
	}

	drogon::app().getLoop()->invalidateTimer(timer_id);
	if (conn && conn->connected())
	{
		conn->shutdown();
	}

	LOG_INFO << "Removed connection for user: " << uid;
	drogon::async_run([self = shared_from_this(), uid, timer_id]() -> drogon::Task<> {
		co_await self->OnUserDisconnected(uid, timer_id);
	});
}

// ────────────────────────────────────────────────────────────
//  Heartbeat monitor & Token refresh
// ────────────────────────────────────────────────────────────

bool ConnectionService::ResetExpiryTimer(const drogon::WebSocketConnectionPtr& conn,
	std::shared_ptr<ConnectionContext>& ctx,
	std::chrono::time_point<std::chrono::system_clock> new_expiry)
{
	// 取消旧定时器
	drogon::app().getLoop()->invalidateTimer(ctx->timer_id);

	auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
		new_expiry - std::chrono::system_clock::now()).count();
	if (remaining <= 10)
		return false;

	auto delay = static_cast<double>(remaining) + Utils::GetRandomJitter();

	auto new_timer_id = drogon::app().getLoop()->runAfter(
		delay, [weak_conn = std::weak_ptr(conn)]()
		{
			auto locked = weak_conn.lock();
			if (locked && locked->connected())
				locked->shutdown(drogon::CloseCode::kNormalClosure, "access token expired");
		});

	ctx->timer_id = new_timer_id;
	ctx->expiry = new_expiry;
	ctx->expiry_warned = false;
	return true;
}

bool ConnectionService::RefreshConnectionToken(const drogon::WebSocketConnectionPtr& conn,
	const std::string& new_access_token)
{
	auto result = Auth::TokenService::GetInstance().Verify(
		new_access_token, Auth::TokenType::Access);
	if (!result)
	{
		LOG_WARN << "[Heartbeat] Token refresh failed: invalid access token";
		return false;
	}

	auto ctx = conn ? conn->getContext<ConnectionContext>() : nullptr;
	if (!ctx)
		return false;

	if (ctx->uid != result->uid)
	{
		LOG_WARN << "[Heartbeat] Token refresh uid mismatch: conn=" << ctx->uid
			<< " token=" << result->uid;
		return false;
	}

	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(ctx->uid);
		if (it == _conn_to_id_map.end() || it->second != conn)
		{
			LOG_WARN << "[Heartbeat] Token refresh failed: connection is no longer current for uid="
				<< ctx->uid;
			return false;
		}
	}

	{
		std::lock_guard<std::mutex> state_lock(ctx->mutex);
		if (!ResetExpiryTimer(conn, ctx, result->expire_at))
		{
			LOG_WARN << "[Heartbeat] New token remaining time too short for uid=" << ctx->uid;
			return false;
		}

		ctx->last_active_time = std::chrono::system_clock::now();
	}

	LOG_INFO << "[Heartbeat] Connection token refreshed for uid=" << ctx->uid
		<< ", new expiry in "
		<< std::chrono::duration_cast<std::chrono::seconds>(
			result->expire_at - std::chrono::system_clock::now()).count()
		<< "s";
	return true;
}

void ConnectionService::StartHeartbeatMonitor()
{
	_monitor_timer_id = drogon::app().getLoop()->runEvery(
		Heartbeat::MonitorIntervalSec,
		[weak_self = weak_from_this()]()
		{
			auto self = weak_self.lock();
			if (self)
				self->RunHeartbeatCheck();
		});
	LOG_INFO << "[Heartbeat] Monitor started: check every "
		<< Heartbeat::MonitorIntervalSec << "s, zombie timeout "
		<< Heartbeat::ZombieTimeoutSec << "s, expiry warning "
		<< Heartbeat::WarningBeforeExpirySec << "s before";
}

void ConnectionService::RunHeartbeatCheck()
{
	const auto now = std::chrono::system_clock::now();
	const auto zombie_threshold = now - std::chrono::duration<double>(Heartbeat::ZombieTimeoutSec);
	const auto warning_threshold = now + std::chrono::duration<double>(Heartbeat::WarningBeforeExpirySec);
	const auto grace_deadline = now - std::chrono::duration<double>(Heartbeat::RefreshGracePeriodSec);

	std::vector<drogon::WebSocketConnectionPtr> snapshot;
	{
		std::lock_guard lock(_mutex);
		snapshot.reserve(_conn_to_id_map.size());
		for (const auto& [uid, conn] : _conn_to_id_map)
		{
			if (conn)
				snapshot.push_back(conn);
		}
	}

	std::vector<drogon::WebSocketConnectionPtr> zombies;
	std::vector<drogon::WebSocketConnectionPtr> grace_expired;
	std::vector<std::pair<drogon::WebSocketConnectionPtr, int64_t>> warn_targets;

	for (const auto& conn : snapshot)
	{
		if (!(conn && conn->connected()))
			continue;

		auto ctx = conn->getContext<ConnectionContext>();
		if (!ctx)
			continue;

		bool is_zombie = false;
		bool is_grace_expired = false;
		int64_t remaining = 0;
		bool should_warn = false;
		std::string uid;
		int64_t inactive_seconds = 0;

		{
			std::lock_guard<std::mutex> state_lock(ctx->mutex);
			uid = ctx->uid;

			if (ctx->last_active_time < zombie_threshold)
			{
				is_zombie = true;
				inactive_seconds = std::chrono::duration_cast<std::chrono::seconds>(
					now - ctx->last_active_time).count();
			}
			else if (ctx->expiry < grace_deadline)
			{
				is_grace_expired = true;
			}
			else if (!ctx->expiry_warned && ctx->expiry < warning_threshold && ctx->expiry > now)
			{
				remaining = std::chrono::duration_cast<std::chrono::seconds>(
					ctx->expiry - now).count();
				ctx->expiry_warned = true;
				should_warn = true;
			}
		}

		if (is_zombie)
		{
			LOG_WARN << "[Heartbeat] Zombie detected: uid=" << uid
				<< ", last active " << inactive_seconds << "s ago";
			zombies.push_back(conn);
			continue;
		}

		if (is_grace_expired)
		{
			LOG_INFO << "[Heartbeat] Grace period expired for uid=" << uid;
			grace_expired.push_back(conn);
			continue;
		}

		if (should_warn)
		{
			warn_targets.emplace_back(conn, remaining);
		}
	}

	for (auto& conn : zombies)
	{
		conn->shutdown(drogon::CloseCode::kNormalClosure, "zombie: no activity");
	}

	for (auto& conn : grace_expired)
	{
		conn->shutdown(drogon::CloseCode::kViolation, "token expired, grace period ended");
	}

	for (auto& [conn, remaining] : warn_targets)
	{
		Json::Value warning;
		warning["type"] = Heartbeat::MsgType::TokenExpiring;
		warning["expires_in"] = static_cast<Json::Int64>(remaining);
		warning["message"] = "access token expiring soon, please refresh";
		Utils::SendJson(conn, warning);
	}
}
