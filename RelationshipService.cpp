#include "pch.h"
#include "RelationshipService.h"

bool RelationshipService::IsPending(const std::string& actor_uid, const std::string& reactor_uid)
{
	return DatabaseManager::ValidateRelationship(actor_uid, reactor_uid, "Pending");
}

bool RelationshipService::IsFriend(const std::string& first_uid, const std::string& second_uid)
{
	return DatabaseManager::ValidateRelationship(first_uid, second_uid, "Friend");

}

bool RelationshipService::IsBlocking(const std::string& actor_uid, const std::string& reactor_uid)
{
	return DatabaseManager::ValidateRelationship(actor_uid, reactor_uid, "Blocking");
}

bool RelationshipService::IsBlockFriend(const std::string& actor_uid, const std::string& reactor_uid)
{
	return DatabaseManager::ValidateRelationship(actor_uid, reactor_uid, "BlockFriend");
}
