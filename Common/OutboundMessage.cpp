#include "pch.h"
#include "OutboundMessage.h"
#include "WebMessageCodec.h"

namespace ChatDelivery
{
    OutboundMessage OutboundMessage::Chat(const ChatMessage& message, DeliveryPolicy policy)
    {
        OutboundMessage outbound;
        outbound.policy = policy;
        outbound.channel = OfflineChannel::Message;
        outbound.payload = message;
        return outbound;
    }

    OutboundMessage OutboundMessage::NoticeMessage(const Notice& notice, DeliveryPolicy policy)
    {
        OutboundMessage outbound;
        outbound.policy = policy;
        outbound.channel = OfflineChannel::Notice;
        outbound.payload = notice;
        return outbound;
    }

    OutboundMessage OutboundMessage::Envelope(const Json::Value& envelope,
        DeliveryPolicy policy,
        OfflineChannel channel)
    {
        OutboundMessage outbound;
        outbound.policy = policy;
        outbound.channel = channel;
        outbound.payload = envelope;
        return outbound;
    }

    Json::Value OutboundMessage::ToEnvelope() const
    {
        if (const auto* chat = std::get_if<ChatMessage>(&payload))
            return ChatCodec::EncodeChatEnvelope(*chat);

        if (const auto* notice = std::get_if<Notice>(&payload))
            return ChatCodec::EncodeNoticeEnvelope(*notice);

        if (const auto* envelope = std::get_if<Json::Value>(&payload))
        {
            return *envelope;
        }

        return Json::nullValue;
    }
}
