#include "pch.h"
#include "NotificationDTO.h"
#include "models/Notifications.h"

using namespace Utils::Notification;
using Notifications = drogon_model::sqlite3::Notifications;

bool NotificationDTO::BuildFromJson(const Json::Value& data, NotificationDTO& dto)
{
	if (!data.isMember("sender_uid") || !data.isMember("receiver_uid") 
		|| !data.isMember("source") || !data.isMember("action_type"))
	{
		return false;
	}

	if (data["sender_uid"].asString() == data["receiver_uid"].asString())
		return false;

	dto.SetSource(StringToType(data["source"].asString()));
	if (!dto.SetNotificationType(data["action_type"].asString()))
		return false;

	dto.SetSenderUid(data["sender_uid"].asString());
	dto.SetReceiverUid(data["receiver_uid"].asString());
	if (data.isMember("content"))
	{
		dto.SetContent(data["content"].asString());
	}
	return true;
}


std::optional<Notifications> NotificationDTO::ToNotifications() const
{
	Notifications notification;
	if (GetNotificationType().has_value())
		notification.setNotificationType(notification_type);
	notification.setActorUid(sender_uid);
	notification.setReactorUid(receiver_uid);
	notification.setNotificationId(notification_id);
	if (content.has_value()) notification.setContent(content.value());
	//notification.setSource(TypeToString(source));
	return notification;
}

void NotificationDTO::SetSenderUid(const std::string& uid)
{
	sender_uid = uid;
}

void NotificationDTO::SetReceiverUid(const std::string& uid)
{
	receiver_uid = uid;
}

bool NotificationDTO::SetNotificationType(const std::string& type)
{
	switch (source)
	{
	case NotificationSource::Unknown:
		return false;
	case NotificationSource::Group:
		break;
	case NotificationSource::System:
		break;
	case NotificationSource::Relationship:
		if (!Utils::UserAction::RelationAction::IsValid(type))
			return false;
		notification_type = type;
		return true;
	}
	return false;
}

void NotificationDTO::SetContent(const std::string& new_content)
{
	this->content = new_content;
}

bool NotificationDTO::SetSource(NotificationSource source)
{
	if (source== NotificationSource::Unknown)
	{
		return false;
	}
	this->source = source;
	return true;
}

std::string NotificationDTO::GetSenderUid() const
{
	return sender_uid;
}

std::string NotificationDTO::GetReceiverUid() const
{
	return receiver_uid;
}

std::optional<std::string> NotificationDTO::GetNotificationType() const
{
	switch (source)
	{
	case NotificationSource::Unknown:
		return std::nullopt;
	case NotificationSource::Relationship:
		if (source == NotificationSource::Unknown)
			return std::nullopt;
		break;
	case NotificationSource::Group:
		break;
	case NotificationSource::System:
		break;
	}

	return notification_type;
}

std::optional<std::string> NotificationDTO::GetContent() const
{
	return content;
}

NotificationSource NotificationDTO::GetSource() const
{
	return source;
}

std::string NotificationDTO::GetNotificationID() const
{
	return notification_id;
}

Json::Value NotificationDTO::TransToJsonMsg() const
{
	Json::Value data;
	data["sender_uid"] = sender_uid;
	data["receiver_uid"] = receiver_uid;
	data["notification_type"] = notification_type;
	data["notification_id"] = notification_id;
	data["source"] = TypeToString(source);
	if (content.has_value()) data["content"] = content.value();
	return data;
}


void NotificationDTO::Clear()
{
	sender_uid.clear();
	receiver_uid.clear();
	notification_type.clear();
	notification_id.clear();
	source = NotificationSource::Unknown;
	content.reset();
}
