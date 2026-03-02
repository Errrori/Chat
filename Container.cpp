#include "pch.h"
#include "Container.h"

#include "Data/SQLiteMessageRepository.h"
#include "Data/SQLiteThreadRepository.h"
#include "Data/SQLiteUserRepository.h"
#include "Data/SQLiteRelationshipRepository.h"
#include "Service/ThreadService.h"
#include "Service/UserService.h"
#include "Service/MessageService.h"
#include "Service/ConnectionService.h"
#include "Service/RelationshipService.h"
#include "controllers/ChatController.h"

Container& Container::GetInstance()
{
	static Container container;
	return container;
}

Container::Container()
{
	_user_repo = std::make_shared<SQLiteUserRepository>();
	_user_service = std::make_shared<UserService>(_user_repo);
	_thread_repo = std::make_shared<SQLiteThreadRepository>();
	_thread_service = std::make_shared<ThreadService>(_thread_repo);
	_message_repo = std::make_shared<SQLiteMessageRepository>();
	_conn_service = std::make_shared<ConnectionService>();
	_message_service = std::make_shared<MessageService>(_message_repo,_conn_service,_thread_service);
	
	_relationship_repo = std::make_shared<SQLiteRelationshipRepository>();
	_relationship_service = std::make_shared<RelationshipService>(_relationship_repo, _conn_service);
}
