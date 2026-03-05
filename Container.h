#pragma once
class IMessageRepository;
class MessageService;
class ThreadService;
class IThreadRepository;
class UserService;
class IUserRepository;
class ConnectionService;
class AuthenticationController;
class ChatController;
class ThreadController;
class RelationshipService;
class IRelationshipRepository;

class Container
{
	friend class ChatController;
	friend class ThreadController;
	friend class AuthController;
	friend class RelationshipController; 
public:
	//register std::unique_ptr for repository and service
	~Container() = default;

	static Container& GetInstance();

	Container(const Container&) = delete;
	Container(Container&&) = delete;
	Container& operator=(const Container&) = delete;
	Container& operator=(Container&&) = delete;

	template<typename T>
	std::shared_ptr<T> GetService() const;

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
};

#define UsingService(T) Container::GetInstance().GetService<T>()

#define GET_USER_SERVICE UsingService(UserService)
#define GET_CONN_SERVICE UsingService(ConnectionService)
#define GET_MESSAGE_SERVICE UsingService(MessageService)
#define GET_THREAD_SERVICE UsingService(ThreadService)
#define GET_RELATIONSHIP_SERVICE UsingService(RelationshipService)

template<>
inline std::shared_ptr<UserService> Container::GetService() const { return _user_service; }

template<>
inline std::shared_ptr<ThreadService> Container::GetService() const { return _thread_service; }

template<>
inline std::shared_ptr<MessageService> Container::GetService() const { return _message_service; }

template<>
inline std::shared_ptr<ConnectionService> Container::GetService() const { return _conn_service; }

template<>
inline std::shared_ptr<RelationshipService> Container::GetService() const { return _relationship_service; }
