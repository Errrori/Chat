#pragma once
#include <memory>

class IMessageRepository;
class MessageService;
class ThreadService;
class IThreadRepository;
class UserService;
class IUserRepository;
class ConnectionService;
class RelationshipService;
class IRelationshipRepository;
class RedisService;

class Container
{
public:
	~Container() = default;

	static Container& GetInstance();

	Container(const Container&) = delete;
	Container(Container&&) = delete;
	Container& operator=(const Container&) = delete;
	Container& operator=(Container&&) = delete;

	std::shared_ptr<UserService>         GetUserService()         const { return _user_service; }
	std::shared_ptr<ThreadService>       GetThreadService()       const { return _thread_service; }
	std::shared_ptr<MessageService>      GetMessageService()      const { return _message_service; }
	std::shared_ptr<ConnectionService>   GetConnectionService()   const { return _conn_service; }
	std::shared_ptr<RelationshipService> GetRelationshipService() const { return _relationship_service; }
	std::shared_ptr<RedisService>        GetRedisService()        const { return _redis_service; }

private:
	Container();

	std::shared_ptr<IUserRepository> _user_repo;
	std::shared_ptr<UserService> _user_service;
	std::shared_ptr<IThreadRepository> _thread_repo;
	std::shared_ptr<ThreadService> _thread_service;
	std::shared_ptr<IMessageRepository> _message_repo;
	std::shared_ptr<MessageService> _message_service;
	std::shared_ptr<ConnectionService> _conn_service;
	std::shared_ptr<IRelationshipRepository> _relationship_repo;
	std::shared_ptr<RelationshipService> _relationship_service;
	std::shared_ptr<RedisService> _redis_service;
};
