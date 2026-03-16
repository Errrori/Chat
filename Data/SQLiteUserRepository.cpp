#include "pch.h"
#include "SQLiteUserRepository.h"
#include "Common/User.h"
#include "models/Users.h"

using namespace drogon::orm;
using Users = drogon_model::sqlite3::Users;

// ─── helpers ────────────────────────────────────────────────────────────────

static UserInfo AuthUserFromRow(const Users& row)
{
	UserInfo info;
	info.setUid(row.getValueOfUid());
	info.setUsername(row.getValueOfUsername());
	info.setAccount(row.getValueOfAccount());
	info.setAvatar(row.getValueOfAvatar());
	return info;
}

static UsersInfo DisplayProfileFromRow(const Users& row)
{
	return UserInfoBuilder::BuildCached(
		row.getValueOfUsername(),
		row.getAvatar() ? row.getValueOfAvatar() : "");
}

static UsersInfo UserProfileFromRow(const Users& row)
{
	return UserInfoBuilder::BuildProfile(
		row.getValueOfUsername(),
		row.getAvatar() ? row.getValueOfAvatar() : "",
		row.getValueOfAccount(),
		row.getValueOfUid(),
		row.getSignature() ? row.getValueOfSignature() : "");
}

static UsersInfo SearchCardFromRow(const Users& row)
{
	return UserInfoBuilder::BuildProfile(
		row.getValueOfUsername(),
		row.getAvatar() ? row.getValueOfAvatar() : "",
		row.getValueOfAccount(),
		row.getValueOfUid(),
		row.getSignature() ? row.getValueOfSignature() : "");
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

drogon::Task<UserInfo> SQLiteUserRepository::GetAuthUserByAccount(const std::string& account) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto result = co_await mapper.limit(1).findBy(
			Criteria(Users::Cols::_account, CompareOperator::EQ, account));
		if (result.empty())
			co_return UserInfo{};
		co_return AuthUserFromRow(result[0]);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "GetAuthUserByAccount error: " << e.what();
		co_return UserInfo{};
	}
}

drogon::Task<UsersInfo> SQLiteUserRepository::GetDisplayProfileByUid(const std::string& uid) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto result = co_await mapper.limit(1).findBy(
			Criteria(Users::Cols::_uid, CompareOperator::EQ, uid));
		if (result.empty())
			co_return UsersInfo{};
		co_return DisplayProfileFromRow(result[0]);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "GetDisplayProfileByUid error: " << e.what();
		co_return UsersInfo{};
	}
}

drogon::Task<UsersInfo> SQLiteUserRepository::GetUserProfileByUid(const std::string& uid) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto result = co_await mapper.limit(1).findBy(
			Criteria(Users::Cols::_uid, CompareOperator::EQ, uid));
		if (result.empty())
			co_return UsersInfo{};
		co_return UserProfileFromRow(result[0]);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "GetUserProfileByUid error: " << e.what();
		co_return UsersInfo{};
	}
}

drogon::Task<UsersInfo> SQLiteUserRepository::FindUserByAccount(const std::string& account) const
{
	try
	{
		CoroMapper<Users> mapper(_db);
		auto result = co_await mapper.limit(1).findBy(
			Criteria(Users::Cols::_account, CompareOperator::EQ, account));
		if (result.empty())
			co_return UsersInfo{};
		co_return SearchCardFromRow(result[0]);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "FindUserByAccount error: " << e.what();
		co_return UsersInfo{};
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

drogon::Task<bool> SQLiteUserRepository::UpdateUserProfile(const std::string& uid, const UsersInfo& update_info)
{
	if (!update_info.HasUpdates())
		co_return false;

	const char* sql =
		"UPDATE users SET "
		"username = COALESCE(?, username), "
		"avatar = COALESCE(?, avatar), "
		"signature = COALESCE(?, signature) "
		"WHERE uid = ?";

	try
	{
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

