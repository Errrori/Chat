#pragma once

#include <json/json.h>

class ChatMessage;
class Notice;

namespace ChatCodec
{
    Json::Value EncodeChatEnvelope(const ChatMessage& message);
    Json::Value EncodeNoticeEnvelope(const Notice& notice);
}
