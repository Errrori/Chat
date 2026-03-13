#include "pch.h"
#include "ConnectionService.h"
#include "RedisService.h"

bool ConnectionService::SendEnvelopeOnline(const std::string& uid, const Json::Value& envelope)
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

	Utils::SendJson(it->second, envelope);
	return true;
}

drogon::Task<bool> ConnectionService::IsOnline(const std::string& uid)
{
	{
		std::lock_guard lock(_mutex);
		if (_conn_to_id_map.contains(uid))
			co_return true;
	}
	co_return co_await _redis_service->IsOnline(uid);
}

bool ConnectionService::AddConnection(const drogon::WebSocketConnectionPtr& conn, const std::string& uid)
{
	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		if (it != _conn_to_id_map.end())
		{
			LOG_INFO << "user already have a connection";
			const auto& old_conn = it->second;
			if (old_conn&&old_conn->connected())
				old_conn->shutdown(drogon::CloseCode::kViolation,"another device log in");
			_conn_to_id_map.erase(it);
		}
		_conn_to_id_map.emplace(uid, conn);
		LOG_INFO << "new user connected: " << uid;
		auto ctx = std::make_shared<ConnectionContext>();
		ctx->uid = uid;
		conn->setContext(ctx);
	}

	const auto connected_uid = conn->getContext<ConnectionContext>()->uid;
	drogon::async_run([this, connected_uid]() -> drogon::Task<> {
		co_await OnUserConnected(connected_uid);
	});

	return true;
}

bool ConnectionService::RemoveConnection(const std::string& uid)
{
	std::lock_guard lock(_mutex);
	auto it = _conn_to_id_map.find(uid);
	if (it == _conn_to_id_map.end())
	{
		LOG_ERROR << "Can not find connection,uid :" << uid;
		return false;
	}

	_conn_to_id_map.erase(uid);

	LOG_INFO << "Remove Connection: " << uid;

	drogon::async_run([this, uid]() -> drogon::Task<> {
		co_await OnUserDisconnected(uid);
	});

	return true;
}


drogon::Task<> ConnectionService::OnUserConnected(std::string uid)
{
	co_await _redis_service->SetOnline(uid);
	auto notices = co_await _redis_service->PopAllOfflineNotices(uid);
	auto msgs = co_await _redis_service->PopAllOfflineMessages(uid);
	if (!notices.empty() || !msgs.empty())
	{
		LOG_INFO << "Delivering offline packets to " << uid
			<< " notice=" << notices.size() << " message=" << msgs.size();
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		if (it != _conn_to_id_map.end() && it->second && it->second->connected())
		{
			for (const auto& notice_str : notices)
				it->second->send(notice_str);
			for (const auto& msg_str : msgs)
				it->second->send(msg_str);
		}
	}
	else
	{
		LOG_INFO << "you don't have offline message";
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
		result.reason = "invalid envelope";
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
		result.reason = "sent";
		co_return result;
	}

	if (message.policy == ChatDelivery::DeliveryPolicy::OnlineOnly)
	{
		result.reason = "offline dropped";
		co_return result;
	}

	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	const auto serialized = Json::writeString(builder, envelope);
	co_await _redis_service->PushOfflinePacket(uid, serialized, message.channel);

	result.state = ChatDelivery::DeliveryState::Queued;
	result.reason = "queued";
	co_return result;
}

drogon::Task<> ConnectionService::OnUserDisconnected(std::string uid)
{
	co_await _redis_service->SetOffline(uid);
}

bool ConnectionService::RemoveConnection(const drogon::WebSocketConnectionPtr& conn)
{
	auto info_ptr = conn->getContext<ConnectionContext>();
	if (!info_ptr)
	{
		LOG_ERROR << "Error to get ConnectionPtr context";
		return false;
	}

	const auto uid = info_ptr->uid;
	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		// 只有当 map 里存的就是这个连接时才删除，防止重连时把新连接误删
		if (it == _conn_to_id_map.end() || it->second != conn)
		{
			LOG_INFO << "Connection already replaced, skip remove for uid: " << uid;
			return false;
		}
		_conn_to_id_map.erase(it);
	}

	LOG_INFO << "Remove Connection: " << uid;
	drogon::async_run([this, uid]() -> drogon::Task<> {
		co_await OnUserDisconnected(uid);
	});
	return true;
}

std::shared_ptr<ConnectionContext> ConnectionService::GetConnInfo(const drogon::WebSocketConnectionPtr& conn) const
{
	if (conn&&conn->connected())
		return conn->getContext<ConnectionContext>();

	return nullptr;
}

bool ConnectionService::SendIfConnected(const std::string& uid, const Json::Value& message)
{
	return SendEnvelopeOnline(uid, message);
}

void ConnectionService::PostNotice(const std::string& receiver_uid, const Json::Value& notice)
{
	drogon::async_run([this, receiver_uid, notice]() -> drogon::Task<>
	{
		co_await DeliverToUser(receiver_uid,
			ChatDelivery::OutboundMessage::Envelope(
				notice,
				ChatDelivery::DeliveryPolicy::MustDeliver,
				ChatDelivery::OfflineChannel::Notice));
	});
}

void ConnectionService::Broadcast(const std::vector<std::string>& targets, const Json::Value& message)
{
	if (targets.empty())
	{
		LOG_INFO << "not target to send";
		return;
	}

	int sent_count = 0;
	for (const auto& uid : targets)
	{
		if (SendEnvelopeOnline(uid, message))
			sent_count++;
	}
	LOG_INFO << "Broadcast complete: " << sent_count << "/" << targets.size() << " messages sent";
}

