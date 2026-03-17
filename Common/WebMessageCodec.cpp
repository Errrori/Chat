#include "pch.h"
#include "WebMessageCodec.h"

#include "ChatMessage.h"
#include "Notice.h"
#include "Enums.h"

namespace ChatCodec
{
    Json::Value EncodeChatEnvelope(const ChatMessage& message)
    {
        Json::Value msg_data;
        msg_data["thread_id"] = message.getThreadId();
        msg_data["message_id"] = static_cast<Json::Value::Int64>(message.getMessageId());
        msg_data["content"] = message.getContent();
        msg_data["attachment"] = message.getAttachment();
        msg_data["sender_uid"] = message.getSenderUid();
        msg_data["sender_name"] = message.getSenderName();
        msg_data["sender_avatar"] = message.getSenderAvatar();
        msg_data["status"] = message.getStatus();
        msg_data["create_time"] = static_cast<Json::Value::Int64>(message.getCreateTime());
        msg_data["update_time"] = static_cast<Json::Value::Int64>(message.getUpdateTime());

        Json::Value msg;
        msg["type"] = static_cast<int>(ChatEnums::WebMessageType::ChatMessage);
        msg["data"] = msg_data;
        return msg;
    }

    Json::Value EncodeNoticeEnvelope(const Notice& notice)
    {
        Json::Value data;
        data["sender_uid"] = notice.getSenderUid();
        data["sender_name"] = notice.getSenderName();
        data["sender_avatar"] = notice.getSenderAvatar();
        data["payload"] = notice.getMessage();
        data["created_time"] = notice.getCreatedTime();
        data["notice_id"] = notice.getNoticeId();
        data["type"] = static_cast<int>(notice.getType());

        Json::Value wrapped;
        wrapped["type"] = static_cast<int>(ChatEnums::WebMessageType::Notice);
        wrapped["data"] = data;
        return wrapped;
    }
}
