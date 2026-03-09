#include "pch.h"
#include "SQLiteUserRepository.h"
#include "Common/User.h"
#include "models/Users.h"

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

	Mapper<Users> mapper(_db);
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

//��ʱֻ֧���޸��������û���������
std::future<bool> SQLiteUserRepository::ModifyUserInfo(const UserInfo& info)
{
	auto promise = std::make_shared<std::promise<bool>>();

	Mapper<Users> mapper(_db);

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
	Mapper<Users> mapper(_db);
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

//���������֪��Ҫ����userinfo����json�ȽϺ�
std::future<Json::Value> SQLiteUserRepository::GetUserInfoByUid(const std::string& uid) const noexcept(false)
{
	auto promise = std::make_shared<std::promise<Json::Value>>();
	Mapper<Users> mapper(_db);
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
	Mapper<Users> mapper(_db);
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

drogon::Task<UserInfo> SQLiteUserRepository::GetUserInfo(const std::string& uid)
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto users = co_await mapper.findBy(Criteria(Users::Cols::_uid, CompareOperator::EQ, uid));

		if (users.empty())
		{
			throw std::runtime_error("User not found");
		}

		auto& db_user = users[0];
		UserInfo user_info;
		user_info.setUsername(db_user.getValueOfUsername());
		user_info.setAvatar(db_user.getValueOfAvatar());
		user_info.setAccount(db_user.getValueOfAccount());
		user_info.setUid(db_user.getValueOfUid());
		user_info.setEmail(db_user.getValueOfEmail());
		if(db_user.getPosts())
			user_info.setPosts(db_user.getValueOfPosts());
		if(db_user.getFollowers())
			user_info.setFollowers(db_user.getValueOfFollowers());
		if(db_user.getFollowing())
			user_info.setFollowing(db_user.getValueOfFollowing());
		if(db_user.getLevel())
			user_info.setLevel(db_user.getValueOfLevel());
		if(db_user.getStatus())
			user_info.setStatus(db_user.getValueOfStatus());
		if(db_user.getCreateTime())
			user_info.setCreateTime(db_user.getValueOfCreateTime());
		if(db_user.getLastLoginTime())
			user_info.setLastLoginTime(db_user.getValueOfLastLoginTime());
		if(db_user.getSignature())
			user_info.setSignature(db_user.getValueOfSignature());

		co_return user_info;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Failed to get user info for uid: " << uid << " - " << e.what();
		throw;
	}
}

drogon::Task<Json::Value> SQLiteUserRepository::GetUserInfoByAccountCoro(const std::string& account) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto users = co_await mapper.findBy(Criteria(Users::Cols::_account, CompareOperator::EQ, account));
		if (users.empty())
			co_return Json::nullValue;
		co_return users[0].toJson();
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "GetUserInfoByAccountCoro error: " << e.what();
		co_return Json::nullValue;
	}
}

drogon::Task<bool> SQLiteUserRepository::AddUserCoro(const UserInfo& info)
{
	const auto db_user_opt = info.ToDbUsers();
	if (!db_user_opt.has_value())
		co_return false;
	try
	{
		CoroMapper<Users> mapper(_db);
		co_await mapper.insert(db_user_opt.value());
		co_return true;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "AddUserCoro error: " << e.what();
		co_return false;
	}
}

drogon::Task<Json::Value> SQLiteUserRepository::GetUserInfoByUidCoro(const std::string& uid) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto users = co_await mapper.findBy(Criteria(Users::Cols::_uid, CompareOperator::EQ, uid));
		if (users.empty())
			co_return Json::nullValue;
		co_return users[0].toJson();
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "GetUserInfoByUidCoro error: " << e.what();
		co_return Json::nullValue;
	}
}
