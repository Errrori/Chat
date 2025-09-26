#include "pch.h"
#include "Container.h"

#include "Data/SQLiteThreadRepository.h"
#include "Data/SQLiteUserRepository.h"
#include "Service/ThreadService.h"
#include "Service/UserService.h"



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
}
