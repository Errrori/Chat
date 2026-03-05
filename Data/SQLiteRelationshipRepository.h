#pragma once
#include "Data/IRelationshipRepository.h"

class SQLiteRelationshipRepository :public IRelationshipRepository
{
public:
	drogon::Task<int64_t> WriteFriendRequest(const std::string& requester_uid,
		const std::string& acceptor_uid, const std::string& payload) override;

	drogon::Task<int64_t> ProcessRequest(const std::string& requester_uid,
	                                     const std::string& acceptor_uid, int status) override;

	drogon::Task<> WriteNotice(const std::string& actor_uid,
		const std::string& reactor_uid, int type, const std::string& payload) override;


	//drogon::Task<>
	//	DeleteFriend(const std::string& requester_uid, const std::string& acceptor_uid) override;

	//drogon::Task<>
	//	GetFriendList(const std::string& uid) override;

};
