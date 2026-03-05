#pragma once
#include <drogon/drogon.h>

class IRelationshipRepository
{
public:
	virtual ~IRelationshipRepository() = default;

	virtual drogon::Task<int64_t>
		WriteFriendRequest(const std::string& requester_uid,const std::string& acceptor_uid,const std::string& payload) = 0;

	virtual drogon::Task<int64_t>
		ProcessRequest(const std::string& requester_uid, const std::string& acceptor_uid, int status) = 0;

	virtual drogon::Task<>
		WriteNotice(const std::string& actor_uid,const std::string& reactor_uid,int type,const std::string& payload) = 0;

	//virtual drogon::Task<>
	//	DeleteFriend(const std::string& requester_uid, const std::string& acceptor_uid) = 0;

	//virtual drogon::Task<>
	//	GetFriendList(const std::string& uid) = 0;
};
