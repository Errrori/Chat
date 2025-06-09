#pragma once

using NotificationSource = Utils::Notification::NotificationSource;

struct NotificationDTO
{
public:
	Json::Value TransToJsonMsg() const;
	std::optional<drogon_model::sqlite3::Notifications> ToNotifications() const;
	static bool BuildFromJson(const Json::Value& data, NotificationDTO& dto);
	template<typename T>
	static std::optional<NotificationDTO> BuildFromDTO(const T& dto,Utils::Notification::NotificationSource source);

	void SetSenderUid(const std::string& uid);
	void SetReceiverUid(const std::string& uid);
	void SetContent(const std::string& new_content);
	bool SetSource(NotificationSource source);
	[[nodiscard]] bool SetNotificationType(const std::string& type);

	std::string GetSenderUid() const;
	std::string GetReceiverUid() const;
	std::optional<std::string> GetNotificationType() const;
	std::optional<std::string> GetContent() const;
	NotificationSource GetSource() const;
	std::string GetNotificationID() const;

	void Clear();

private:
	std::string sender_uid;
	std::string receiver_uid;
	std::string notification_id;
	std::string notification_type;//ºóÐø»»³É std::variant
	NotificationSource source = NotificationSource::Unknown;
	std::optional<std::string> content;
};

template <typename T>
std::optional<NotificationDTO> NotificationDTO::BuildFromDTO(const T& dto, Utils::Notification::NotificationSource source)
{
	NotificationDTO notification_dto;
	switch (source)
	{
	case NotificationSource::Unknown:
		return std::nullopt;
	case NotificationSource::Group:
		break;
	case NotificationSource::System:
		break;
	case NotificationSource::Relationship:
		notification_dto.SetSource(source);
		if (!notification_dto.SetNotificationType(Utils::UserAction::RelationAction::TypeToString(dto.GetActionType())))
		{
			LOG_ERROR << "can not set notification type: " << Utils::UserAction::RelationAction::TypeToString(dto.GetActionType());
			return std::nullopt;
		}
		notification_dto.SetSenderUid(dto.GetActorUid());
		notification_dto.SetReceiverUid(dto.GetReactorUid());
		notification_dto.SetContent(dto.GetContent().value());
		notification_dto.notification_id = Utils::Authentication::GenerateUid();
		return notification_dto;
	}
	return std::nullopt;
}

