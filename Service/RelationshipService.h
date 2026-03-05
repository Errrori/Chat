#pragma once
#include <drogon/drogon.h>
#include <memory>

class IRelationshipRepository;
class ConnectionService;

class RelationshipService
{
	friend class Container;
public:
	RelationshipService(std::shared_ptr<IRelationshipRepository> repo, std::shared_ptr<ConnectionService> connService);

	drogon::Task<> SendFriendRequest(const std::string& requester_uid,
		const std::string& acceptor_uid, const std::string& message) const;
	drogon::Task<int64_t> ProcessFriendRequest(const std::string& requester_uid,
	                                           const std::string& acceptor_uid, int action) const;

private:
	std::shared_ptr<IRelationshipRepository> _relationship_repo;
	std::shared_ptr<ConnectionService> _conn_service;
};
