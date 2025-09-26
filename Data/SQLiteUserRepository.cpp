#include "pch.h"
#include "SQLiteUserRepository.h"
#include "Common/User.h"
#include "DbAccessor.h"

using namespace drogon::orm;
using Users = drogon_model::sqlite3::Users;


std::future<bool> SQLiteUserRepository::AddUser(const UserInfo& info)
{
	const auto& db_user_opt = info.ToDbUsers();
	if (!db_user_opt.has_value())
	{
		std::promise<bool> promise;
		auto future = promise.get_future();
		promise.set_value(false);
		return future;
	}

	auto promise = std::make_shared<std::promise<bool>>();

	Mapper<Users> mapper(DbAccessor::GetDbClient());
	mapper.insert(db_user_opt.value(),
		[promise](const Users& user)
		{
			promise->set_value(true);
		},
		[promise](const DrogonDbException& e)
		{
			promise->set_value(false);
			LOG_ERROR << "can not insert user info: " << e.base().what();
		});
	return promise->get_future();
}

//暂时只支持修改姓名，用户名和密码
std::future<bool> SQLiteUserRepository::ModifyUserInfo(const UserInfo& info)
{
	auto promise = std::make_shared<std::promise<bool>>();

	Mapper<Users> mapper(DbAccessor::GetDbClient());

	Criteria criteria(Users::Cols::_uid,CompareOperator::EQ,info.getUid());

	mapper.updateBy({Users::Cols::_avatar,Users::Cols::_username,Users::Cols::_password},
		[promise](size_t size) 
		{
			if (size == 1)
				promise->set_value(true);
			else
				promise->set_value(false);
		},
		[promise](const DrogonDbException& e)
		{
			LOG_ERROR << "can not modify user info, exception: " << e.base().what();
			promise->set_value(false);
		},
		criteria,
		info.getAvatar(),info.getUsername(),info.getHashedPassword());

	return promise->get_future();
}

std::future<bool> SQLiteUserRepository::DeleteUser(const std::string& uid)
{
	auto promise = std::make_shared<std::promise<bool>>();
	Mapper<Users> mapper(DbAccessor::GetDbClient());
	Criteria criteria(Users::Cols::_uid, CompareOperator::EQ, uid);
	mapper.deleteBy(criteria,
		[promise](size_t size)
		{
			if (size == 1)
				promise->set_value(true);
			else
				promise->set_value(false);
		},
		[promise](const DrogonDbException& e)
		{
			LOG_ERROR << "can not delete user, exception: " << e.base().what();
			promise->set_value(false);
		}
	);
	return promise->get_future();
}

//这个函数不知道要返回userinfo还是json比较好
std::future<Json::Value> SQLiteUserRepository::GetUserInfoByUid(const std::string& uid) const noexcept(false)
{
	auto promise = std::make_shared<std::promise<Json::Value>>();
	Mapper<Users> mapper(DbAccessor::GetDbClient());
	Criteria criteria(Users::Cols::_uid, CompareOperator::EQ, uid);
	mapper.findBy(criteria,
		[promise](const std::vector<Users>& results)
		{
			if (results.size() == 1)
				promise->set_value(results[0].toJson());
			else
				promise->set_value(Json::nullValue);
		},
		[promise](const DrogonDbException& e)
		{
			LOG_ERROR << "can not delete user, exception: " << e.base().what();
			promise->set_exception(std::make_exception_ptr(e));
		});
	return promise->get_future();
}

std::future<Json::Value> SQLiteUserRepository::GetUserInfoByAccount(const std::string& account) const noexcept(false)
{
	auto promise = std::make_shared<std::promise<Json::Value>>();
	Mapper<Users> mapper(DbAccessor::GetDbClient());
	Criteria criteria(Users::Cols::_account, CompareOperator::EQ, account);
	mapper.findBy(criteria,
		[promise](const std::vector<Users>& results)
		{
			if (results.size() == 1)
				promise->set_value(results[0].toJson());
			else
				promise->set_value(Json::nullValue);
		},
		[promise](const DrogonDbException& e)
		{
			LOG_ERROR << "can not find user, exception: " << e.base().what();
			promise->set_exception(std::make_exception_ptr(e));
		});
	return promise->get_future();
}
