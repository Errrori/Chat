#pragma once

#include <json/json.h>
#include <string>
#include <variant>

#include "Common/ChatMessage.h"
#include "Common/Notice.h"

namespace ChatDelivery
{
    enum class DeliveryPolicy
    {
        OnlineOnly,
        PreferOnline,
        MustDeliver
    };

    enum class OfflineChannel
    {
        Message,
        Notice
    };

    enum class DeliveryState
    {
        Sent,
        Queued,
        Dropped
    };

    struct DeliveryResult
    {
        DeliveryState state{DeliveryState::Dropped};
        std::string reason;

        bool IsSent() const { return state == DeliveryState::Sent; }
        bool IsQueued() const { return state == DeliveryState::Queued; }
    };

    using OutboundPayload = std::variant<ChatMessage, Notice, Json::Value>;

    struct OutboundMessage
    {
        DeliveryPolicy policy{DeliveryPolicy::PreferOnline};
        OfflineChannel channel{OfflineChannel::Message};
        OutboundPayload payload;

        static OutboundMessage Chat(const ChatMessage& message,
            DeliveryPolicy policy = DeliveryPolicy::PreferOnline);

        static OutboundMessage NoticeMessage(const Notice& notice,
            DeliveryPolicy policy = DeliveryPolicy::MustDeliver);

        static OutboundMessage Envelope(const Json::Value& envelope,
            DeliveryPolicy policy,
            OfflineChannel channel);

        Json::Value ToEnvelope() const;
    };
}
