#include "pch.h"
#include "ClusterService.h"
#include "RedisService.h"
#include "Common/ClusterConfig.h"
#include <drogon/utils/coroutine.h>

ClusterService::ClusterService(std::shared_ptr<RedisService> redis, std::string node_id)
    : _redis_service(std::move(redis)), _node_id(std::move(node_id))
{
}

void ClusterService::Start()
{
    RegisterSelf();
    StartHeartbeat();
    SubscribeSelf();

    LOG_INFO << "[Cluster] node started, id=" << _node_id
        << ", channel=" << Cluster::Channel(_node_id)
        << ", heartbeat=" << Cluster::NodeHeartbeatSec
        << "s, alive_ttl=" << Cluster::NodeAliveTTL << "s";
}

void ClusterService::RegisterSelf()
{
    drogon::async_run([self = shared_from_this()]() -> drogon::Task<>
    {
        co_await self->_redis_service->RegisterNode(self->_node_id);
    });
}

void ClusterService::StartHeartbeat()
{
    _heartbeat_timer = drogon::app().getLoop()->runEvery(
        Cluster::NodeHeartbeatSec,
        [weak_self = weak_from_this()]()
        {
            auto self = weak_self.lock();
            if (!self)
                return;

            drogon::async_run([self]() -> drogon::Task<>
            {
                co_await self->_redis_service->RegisterNode(self->_node_id);
            });
        });
}

void ClusterService::SubscribeSelf()
{
    if (!_redis_service)
        return;

    _subscriber = _redis_service->GetClient()->newSubscriber();
    _subscriber->subscribe(
        Cluster::Channel(_node_id),
        [weak_self = weak_from_this()](const std::string& channel, const std::string& message)
        {
            auto self = weak_self.lock();
            if (self)
                self->OnClusterMessage(channel, message);
        });
}

void ClusterService::OnClusterMessage(const std::string& channel, const std::string& message)
{
    Json::Value payload;
    Json::Reader reader;
    if (!reader.parse(message, payload))
    {
        LOG_ERROR << "[Cluster] failed to parse cross-node message on channel " << channel;
        return;
    }

    if (!payload.isMember("target_uid") || !payload.isMember("envelope"))
    {
        LOG_ERROR << "[Cluster] malformed cross-node message on channel " << channel;
        return;
    }

    const auto uid = payload["target_uid"].asString();
    const auto policy = static_cast<ChatDelivery::DeliveryPolicy>(
        payload.get("policy", static_cast<int>(ChatDelivery::DeliveryPolicy::PreferOnline)).asInt());
    const auto channel_kind = static_cast<ChatDelivery::OfflineChannel>(
        payload.get("channel", static_cast<int>(ChatDelivery::OfflineChannel::Message)).asInt());

    LOG_DEBUG << "[Cluster] received cross-node message for uid=" << uid;

    if (_local_deliverer)
        _local_deliverer(uid, payload["envelope"], policy, channel_kind);
}

drogon::Task<bool> ClusterService::RouteToNode(const std::string& node_id,
                                               const std::string& target_uid,
                                               const ChatDelivery::OutboundMessage& message) const
{
    auto envelope = message.ToEnvelope();
    if (envelope.isNull())
        co_return false;

    Json::Value payload;
    payload["target_uid"] = target_uid;
    payload["policy"] = static_cast<int>(message.policy);
    payload["channel"] = static_cast<int>(message.channel);
    payload["envelope"] = envelope;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    const auto serialized = Json::writeString(builder, payload);

    const auto ok = co_await _redis_service->PublishToNode(node_id, serialized);
    if (ok)
    {
        LOG_DEBUG << "[Cluster] routed message for uid=" << target_uid
            << " to node=" << node_id;
    }
    else
    {
        LOG_WARN << "[Cluster] failed to route message for uid=" << target_uid
            << " to node=" << node_id;
    }
    co_return ok;
}

drogon::Task<std::optional<std::string>> ClusterService::LocateUser(const std::string& uid) const
{
    co_return co_await _redis_service->GetUserRoute(uid);
}

drogon::Task<bool> ClusterService::IsNodeAlive(const std::string& node_id) const
{
    co_return co_await _redis_service->IsNodeAlive(node_id);
}
