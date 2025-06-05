#pragma once
class RelationshipService
{
public:
	static bool IsPending(const std::string& actor_uid,const std::string& reactor_uid);
	static bool IsFriend(const std::string& first_uid, const std::string& second_uid);
	static bool IsBlocking(const std::string& actor_uid, const std::string& reactor_uid);
	static bool IsBlockFriend(const std::string& actor_uid, const std::string& reactor_uid);

};

