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

bool ConnectionService::AddConnection(const drogon::WebSocketConnectionPtr& conn, const std::string& uid, 
	std::chrono::time_point<std::chrono::system_clock> expiry)
{
	try
	{
		{
			std::lock_guard lock(_mutex);
			auto it = _conn_to_id_map.find(uid);
			if (it != _conn_to_id_map.end())
			{
				LOG_INFO << "user already have a connection";
				const auto& old_conn = it->second;
				if (old_conn && old_conn->connected())
				{
					LOG_INFO << "kick the old connection";
					old_conn->shutdown(drogon::CloseCode::kViolation, "another device log in");
				}
				_conn_to_id_map.erase(it);
			}
			_conn_to_id_map.emplace(uid, conn);
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
						auto lock = weak_conn.lock();

						if (lock && lock->connected())
						{
							lock->shutdown();
						}
						LOG_ERROR << "Timer not reclaimed in time: "<<lock->getContext<ConnectionContext>()->timer_id;
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
			conn->setContext(std::move(ctx));
		

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
	auto msgs = co_await _redis_service->PopAllOfflineMessages(uid);
	if (!notices.empty() || !msgs.empty())
	{
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

drogon::Task<> ConnectionService::OnUserDisconnected(std::string uid, trantor::TimerId timer_id) const
{
	drogon::app().getLoop()->invalidateTimer(timer_id);
	co_await _redis_service->SetOffline(uid);
}

bool ConnectionService::RemoveConnection(const drogon::WebSocketConnectionPtr& conn)
{
	auto context = conn->getContext<ConnectionContext>();
	if (!context)
	{
		LOG_ERROR << "Error to get ConnectionPtr context";
		return false;
	}

	const auto uid = context->uid;

	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		if (it !=_conn_to_id_map.end()&&conn==it->second)
			_conn_to_id_map.erase(it);
	}

	LOG_INFO << "Remove Connection: " << uid;
	drogon::async_run([self = shared_from_this(), uid,timer_id = context->timer_id]() -> drogon::Task<> {
		co_await self->OnUserDisconnected(uid,timer_id );
	});
	return true;
}

bool ConnectionService::RemoveConnection(const std::string& uid)
{
	trantor::TimerId timer_id;
	{
		std::lock_guard lock(_mutex);
		auto it = _conn_to_id_map.find(uid);
		if (it != _conn_to_id_map.end())
		{
			_conn_to_id_map.erase(it);
			const auto& context = it->second->getContext<ConnectionContext>();
			timer_id = context->timer_id;
		}
		else
			return false;
	}
	drogon::async_run([self = shared_from_this(), uid, timer_id = timer_id]() -> drogon::Task<> {
		co_await self->OnUserDisconnected(uid, timer_id);
		});
	return true;
}

std::shared_ptr<ConnectionContext> ConnectionService::GetConnInfo(const drogon::WebSocketConnectionPtr& conn) const
{
	if (conn&&conn->connected())
		return conn->getContext<ConnectionContext>();

	return nullptr;
}

void ConnectionService::RemoveUserConn(const std::string& uid)
{
	std::lock_guard lock(_mutex);
	auto it = _conn_to_id_map.find(uid);
	if (it != _conn_to_id_map.end())
	{
		it->second->shutdown();
		_conn_to_id_map.erase(it);
		LOG_INFO << "Removed connection for user: " << uid;
	}
}
