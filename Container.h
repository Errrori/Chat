#pragma once
class ThreadService;
class IThreadRepository;
class UserService;
class IUserRepository;

class Container
{
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

};

template<>
inline std::shared_ptr<UserService> Container::GetService() const { return _user_service; }

template<>
inline std::shared_ptr<ThreadService> Container::GetService() const { return _thread_service; }