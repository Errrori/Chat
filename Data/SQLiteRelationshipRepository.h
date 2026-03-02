#pragma once
#include "Data/IRelationshipRepository.h"

class SQLiteRelationshipRepository :public IRelationshipRepository
{
public:
	drogon::Task<>
		WriteFriendRequest(const std::string& requester_uid, const std::string& acceptor_uid, const std::string& payload) override;

	drogon::Task<int64_t>
		ProcessRequest(const std::string& requester_uid, const std::string& acceptor_id, int status, const std::string& payload) override;

	//drogon::Task<>
	//	DeleteFriend(const std::string& requester_uid, const std::string& acceptor_id) override;

	//drogon::Task<>
	//	GetFriendList(const std::string& uid) override;

};
