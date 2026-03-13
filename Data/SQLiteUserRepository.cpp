#include "pch.h"
#include "SQLiteUserRepository.h"
#include "Common/User.h"
#include "models/Users.h"

using namespace drogon::orm;
using Users = drogon_model::sqlite3::Users;

// ─── helpers ────────────────────────────────────────────────────────────────

static UserInfo UserInfoFromRow(const Users& row)
{
	UserInfo info;
	info.setUid(row.getValueOfUid());
	info.setUsername(row.getValueOfUsername());
	info.setAccount(row.getValueOfAccount());
	info.setAvatar(row.getValueOfAvatar());
	return info;
}

// ─── interface implementations ──────────────────────────────────────────────

drogon::Task<std::string> SQLiteUserRepository::GetHashedPassword(const std::string& account) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto result = co_await mapper.limit(1).findBy(
			Criteria(Users::Cols::_account, CompareOperator::EQ, account));
		if (result.empty())
			co_return std::string{};
		co_return result[0].getValueOfPassword();
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "GetHashedPassword error: " << e.what();
		co_return std::string{};
	}
}

drogon::Task<UserInfo> SQLiteUserRepository::GetUserByUid(const std::string& uid) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto result = co_await mapper.limit(1).findBy(
			Criteria(Users::Cols::_uid, CompareOperator::EQ, uid));
		if (result.empty())
			co_return UserInfo{};
		co_return UserInfoFromRow(result[0]);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "GetUserByUid error: " << e.what();
		co_return UserInfo{};
	}
}

drogon::Task<UserInfo> SQLiteUserRepository::GetUserByAccount(const std::string& account) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto result = co_await mapper.limit(1).findBy(
			Criteria(Users::Cols::_account, CompareOperator::EQ, account));
		if (result.empty())
			co_return UserInfo{};
		co_return UserInfoFromRow(result[0]);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "GetUserByAccount error: " << e.what();
		co_return UserInfo{};
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

