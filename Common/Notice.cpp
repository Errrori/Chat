#include "pch.h"
#include "Notice.h"
#include "WebMessageCodec.h"

Notice Notice::FromJson(const Json::Value& json)
{
    Notice notice;
    if (json.isNull() || !json.isObject()) return notice;

    // Check if it's the wrapper format or direct data
    // Assuming we parse the inner "data" if "type" exists, or the object itself if it looks like data
    const Json::Value* dataNode = &json;
    if (json.isMember("type") && json.isMember("data"))
    {
        dataNode = &json["data"];
    }

    if (dataNode->isMember("sender_uid")) notice.setSenderUid((*dataNode)["sender_uid"].asString());
    if (dataNode->isMember("sender_name")) notice.setSenderName((*dataNode)["sender_name"].asString());
    if (dataNode->isMember("sender_avatar")) notice.setSenderAvatar((*dataNode)["sender_avatar"].asString());
    if (dataNode->isMember("message")) notice.setMessage((*dataNode)["message"].asString());
    if (dataNode->isMember("created_time")) notice.setCreatedTime((*dataNode)["created_time"].asInt64());
    if (dataNode->isMember("event_id")) notice.setNoticeId((*dataNode)["event_id"].asInt64());
    if (dataNode->isMember("type")) notice.setType(static_cast<ChatEnums::NoticeType>((*dataNode)["type"].asInt()));

    return notice;
}

Json::Value Notice::ToJson() const
{
    return ChatCodec::EncodeNoticeEnvelope(*this);
}

bool Notice::IsValid() const
{
    return !_sender_uid.empty() && !_message.empty();
}
