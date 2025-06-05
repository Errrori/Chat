#pragma once
#include "Utils.h"
//¹æ¶¨£º
//pending/refuse: first_uid is actor,second_uid is reactor
//friend: first_uid is the smaller uid,second_uid is the bigger uid
//block/block_friend: first_uid is actor,second_uid is reactor

//pending/refuse/blocking/block_friend/friend

namespace drogon_model::sqlite3
{
	class Relationships;
}

struct RelationshipDTO
{
public:
	RelationshipDTO() = default;
	static bool BuildFromJson(const Json::Value& data, RelationshipDTO& dto);
	Json::Value TransToJson() const;
	std::optional<drogon_model::sqlite3::Relationships> ToRelationshipJson(bool& is_deleted) const;

	void SetActorUid(const std::string& uid);
	void SetReactorUid(const std::string& uid);
	bool SetActionType(Utils::UserAction::RelationAction::RelationshipActionType new_type);
	void SetContent(const std::string& content);

	std::string GetActorUid() const;
	std::string GetReactorUid() const;
	Utils::UserAction::RelationAction::RelationshipActionType GetActionType() const;
	std::optional<std::string> GetContent() const;

	void Clear();

private:
	std::string actor_uid;
	std::string reactor_uid;
	Utils::UserAction::RelationAction::RelationshipActionType action_type = Utils::UserAction::RelationAction::RelationshipActionType::Unknown;
	std::optional<std::string> content;
};

