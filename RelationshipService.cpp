#include "pch.h"
#include "RelationshipService.h"

using Status = Utils::Relationship::StatusType;

bool RelationshipService::IsPending(const std::string& actor_uid, const std::string& reactor_uid)
{
	return DatabaseManager::ValidateRelationship(actor_uid, reactor_uid, Utils::Relationship::StatusType::Pending);
}

bool RelationshipService::IsFriend(const std::string& first_uid, const std::string& second_uid)
{
	return DatabaseManager::ValidateRelationship(first_uid, second_uid, Utils::Relationship::StatusType::Friend);

}

bool RelationshipService::IsBlocking(const std::string& actor_uid, const std::string& reactor_uid)
{
	return DatabaseManager::ValidateRelationship(actor_uid, reactor_uid, Utils::Relationship::StatusType::Blocking);
}
