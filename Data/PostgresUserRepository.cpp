#include "pch.h"
#include "PostgresUserRepository.h"
#include "models/Users.h"
#include <drogon/orm/CoroMapper.h>

#include "Common/User.h"

using namespace drogon::orm;
using Users = drogon_model::postgres::Users;

// ─── interface implementations ──────────────────────────────────────────────

drogon::Task<std::string> PostgresUserRepository::GetHashedPassword(const std::string& account) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto rows = co_await mapper.limit(1).findBy(
			Criteria(Users::Cols::_account, CompareOperator::EQ, account));
		if (rows.empty())
			co_return std::string{};
		const auto& pw = rows[0].getPassword();
		co_return pw ? *pw : std::string{};
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "GetHashedPassword error: " << e.what();
		co_return std::string{};
	}
}

drogon::Task<UserInfo> PostgresUserRepository::GetAuthUserByAccount(const std::string& account) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto rows = co_await mapper.limit(1).findBy(
			Criteria(Users::Cols::_account, CompareOperator::EQ, account));
		if (rows.empty())
			co_return UserInfo{};
		const auto& u = rows[0];
		co_return UserInfoBuilder::BuildProfile(
			u.getUsername()  ? *u.getUsername()  : "",
			u.getAvatar()    ? *u.getAvatar()    : "",
			u.getAccount()   ? *u.getAccount()   : "",
			u.getUid()       ? *u.getUid()       : "",
			u.getSignature() ? *u.getSignature() : "");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "GetAuthUserByAccount error: " << e.what();
		co_return UserInfo{};
	}
}

drogon::Task<UserInfo> PostgresUserRepository::GetDisplayProfileByUid(const std::string& uid) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto rows = co_await mapper.limit(1).findBy(
			Criteria(Users::Cols::_uid, CompareOperator::EQ, uid));
		if (rows.empty())
			co_return UserInfo{};
		const auto& u = rows[0];
		co_return UserInfoBuilder::BuildCached(
			u.getUsername() ? *u.getUsername() : "",
			u.getAvatar() ? *u.getAvatar() : "");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "GetDisplayProfileByUid error: " << e.what();
		co_return UserInfo{};
	}
}

drogon::Task<UserInfo> PostgresUserRepository::GetUserProfileByUid(const std::string& uid) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto rows = co_await mapper.limit(1).findBy(
			Criteria(Users::Cols::_uid, CompareOperator::EQ, uid));
		if (rows.empty())
			co_return UserInfo{};
		const auto& u = rows[0];
		co_return UserInfoBuilder::BuildProfile(
			u.getUsername() ? *u.getUsername() : "",
			u.getAvatar() ? *u.getAvatar() : "",
			u.getAccount() ? *u.getAccount() : "",
			u.getUid() ? *u.getUid() : "",
			u.getSignature() ? *u.getSignature() : "");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "GetUserProfileByUid error: " << e.what();
		co_return UserInfo{};
	}
}

drogon::Task<UserInfo> PostgresUserRepository::FindUserByAccount(const std::string& account) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto rows = co_await mapper.limit(1).findBy(
			Criteria(Users::Cols::_account, CompareOperator::EQ, account));
		if (rows.empty())
			co_return UserInfo{};
		const auto& u = rows[0];
		co_return UserInfoBuilder::BuildProfile(
			u.getUsername() ? *u.getUsername() : "",
			u.getAvatar() ? *u.getAvatar() : "",
			u.getAccount() ? *u.getAccount() : "",
			u.getUid() ? *u.getUid() : "",
			u.getSignature() ? *u.getSignature() : "");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "FindUserByAccount error: " << e.what();
		co_return UserInfo{};
	}
}

drogon::Task<bool> PostgresUserRepository::AddUserCoro(const UserInfo& info)
{
	try
	{
		CoroMapper<Users> mapper(_db);
		Users user;
		user.setUid(*info.GetUid());
		user.setAccount(*info.GetAccount());
		user.setUsername(*info.GetUsername());
		user.setPassword(*info.GetHashedPassword());
		user.setAvatar(info.GetAvatar().has_value() ? *info.GetAvatar() : "");
		co_await mapper.insert(user);

		co_return true;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "AddUser error: " << e.what();
		co_return false;
	}
}

drogon::Task<bool> PostgresUserRepository::UpdateUserProfile(const std::string& uid, const UserInfo& update_info)
{
	try
	{
		const std::string sql =
			"UPDATE users SET "
			"username = COALESCE($1, username), "
			"avatar = COALESCE($2, avatar), "
			"signature = COALESCE($3, signature) "
			"WHERE uid = $4";

		auto result = co_await _db->execSqlCoro(
			sql,
			update_info.GetUsername().has_value() ? update_info.GetUsername()->c_str() : nullptr,
			update_info.GetAvatar().has_value() ? update_info.GetAvatar()->c_str() : nullptr,
			update_info.GetSignature().has_value() ? update_info.GetSignature()->c_str() : nullptr,
			uid);
		co_return result.affectedRows() > 0;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "UpdateUserProfile error: " << e.what();
		co_return false;
	}
}

