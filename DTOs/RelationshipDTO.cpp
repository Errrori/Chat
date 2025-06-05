#include "pch.h"
#include "RelationshipDTO.h"
#include "models/Relationships.h"

using namespace Utils::UserAction;
using RelationshipActionType = RelationAction::RelationshipActionType;
bool RelationshipDTO::BuildFromJson(const Json::Value& data, RelationshipDTO& dto)
{
	dto.Clear();
	if (!data.isMember("sender_uid") || !data.isMember("receiver_uid") || !data.isMember("action_type"))
		return false;

	if (data["sender_uid"].asString() == data["receiver_uid"].asString()) 
		return false;

	if (!dto.SetActionType(RelationAction::StringToType(data["action_type"].asString())))
		return false;

	if (data.isMember("content"))
		dto.SetContent(data["content"].asString());

	dto.SetActorUid(data["sender_uid"].asString());
	dto.SetReactorUid(data["receiver_uid"].asString());
	return true;
}

void RelationshipDTO::SetActorUid(const std::string& uid)
{
	actor_uid = uid;
}

void RelationshipDTO::SetReactorUid(const std::string& uid)
{
	reactor_uid = uid;
}

bool RelationshipDTO::SetActionType(RelationshipActionType new_type)
{
	if (new_type== RelationshipActionType::Unknown)
	{
		return false;
	}
	action_type = new_type;
	return true; 
}

void RelationshipDTO::SetContent(const std::string& content)
{
	this->content = content;
}


std::string RelationshipDTO::GetActorUid() const
{
	return actor_uid;
}

std::string RelationshipDTO::GetReactorUid() const
{
	return reactor_uid;
}

RelationshipActionType RelationshipDTO::GetActionType() const
{
	return action_type;
}

std::optional<std::string> RelationshipDTO::GetContent() const
{
	return content;
}


Json::Value RelationshipDTO::TransToJson() const
{
	Json::Value data;
	data["actor_uid"] = actor_uid;
	data["reactor_uid"] = reactor_uid;
	data["action_type"] = RelationAction::TypeToString(action_type);
	return data;
}

std::optional<drogon_model::sqlite3::Relationships> RelationshipDTO::ToRelationshipJson(bool& is_deleted) const
{
	if (actor_uid==reactor_uid||actor_uid.empty()||reactor_uid.empty()
		||action_type== RelationshipActionType::Unknown)
	{
		is_deleted = false;
		return std::nullopt;
	}

	auto status = Utils::UserAction::RelationAction::ToStatus(action_type);
	if (!status.has_value())
	{
		is_deleted = true;
		return std::nullopt;
	}
	is_deleted = false;

	Json::Value data;
	data["first_uid"] = actor_uid;
	data["second_uid"] = reactor_uid;
	data["status"] = Utils::Relationship::TypeToString(status.value());
	drogon_model::sqlite3::Relationships relationship(data);
	return relationship;
}


void RelationshipDTO::Clear()
{
	actor_uid.clear();
	reactor_uid.clear();
	action_type = RelationshipActionType::Unknown;
}