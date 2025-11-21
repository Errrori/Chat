#pragma once


class IRelationshipRepository
{
public:
	~IRelationshipRepository() = default;

	drogon::Task<> WriteFriendRequest(const std::string& requester_uid,const);

};