#pragma once
#include <drogon/drogon.h>
#include "const.h"

struct NotificationDTO;
struct RelationshipDTO;

namespace drogon_model::sqlite3
{
	class Relationships;
	class ChatRecords;
	class Notifications;
	class threads;
	class private_threads;
	class groups;
	class group_members;
	class messages;
}

class DatabaseManager
{
public:
	DatabaseManager() = delete;

	static void InitDatabase();
	static drogon::orm::DbClientPtr GetDbClient();

	static bool PushUser(const Json::Value& user_info);
	static void PushChatRecords(const drogon_model::sqlite3::ChatRecords& chat_record);
	static void PushNotification(const Json::Value& data);
	static bool WriteRelationship(const RelationshipDTO& dto, std::string& error_msg);
	static bool UpsertRelationship
	(const std::string& first_uid, const std::string& second_uid, Utils::Relationship::StatusType status, std::string& error_msg);
	static bool WriteNotification(const NotificationDTO& dto);

	static Json::Value GetAllUsersInfo();
	static Json::Value GetChatRecords(int64_t existing_id, unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static Json::Value GetAllRecords(unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static Json::Value GetMessages(int64_t existing_id, unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static Json::Value GetAllMessage(unsigned num = DataBase::DEFAULT_RECORDS_QUERY_LEN);
	static Json::Value GetRelationships();
	static Json::Value GetNotifications();
	static bool GetUserInfoByUid(const std::string& uid, Json::Value& data);
	static bool GetUserInfoByAccount(const std::string& account, Json::Value& data);
	static bool GetUserNotification(const std::string& uid, Json::Value& data);


	static bool ModifyAvatar(const std::string& uid, const std::string& avatar);
	static bool ModifyUsername(const std::string& uid, const std::string& username);
	static bool ModifyPassword(const std::string& uid, const std::string& password);

	static bool DeleteUser(const std::string& uid);
	static bool DeleteRelationship
	(const std::string& first_uid, const std::string& second_uid, Utils::Relationship::StatusType status, std::string& error_msg);
	static bool ValidateAccount(const std::string& account);
	static bool ValidateUid(const std::string& uid);
	static bool ValidateRelationship(const std::string& actor_uid, const std::string& reactor_uid, Utils::Relationship::StatusType status);

	static std::vector<std::string> GetGroupMember(const std::string& group_id);
	static bool PushMessage(const TransMsg& msg);

	static void CreatePublicThread();
};

