#include "pch.h"
#include "SQLiteUserRepository.h"
#include "Common/User.h"

using namespace drogon::orm;

// ─── helpers ────────────────────────────────────────────────────────────────

namespace
{
std::string PlaceholderFor(drogon::orm::ClientType clientType, int index)
{
	if (clientType == drogon::orm::ClientType::PostgreSQL)
		return "$" + std::to_string(index);
	return "?";
}

std::string NullableText(const drogon::orm::Row& row, const char* col)
{
	if (row[col].isNull())
		return "";
	return row[col].as<std::string>();
}
}

static UserInfo AuthUserFromRow(const drogon::orm::Row& row)
{
	UserInfo info;
	info.setUid(row["uid"].as<std::string>());
	info.setUsername(row["username"].as<std::string>());
	info.setAccount(row["account"].as<std::string>());
	info.setAvatar(NullableText(row, "avatar"));
	return info;
}

static UsersInfo DisplayProfileFromRow(const drogon::orm::Row& row)
{
	return UserInfoBuilder::BuildCached(
		row["username"].as<std::string>(),
		NullableText(row, "avatar"));
}

static UsersInfo UserProfileFromRow(const drogon::orm::Row& row)
{
	return UserInfoBuilder::BuildProfile(
		row["username"].as<std::string>(),
		NullableText(row, "avatar"),
		row["account"].as<std::string>(),
		row["uid"].as<std::string>(),
		NullableText(row, "signature"));
}

static UsersInfo SearchCardFromRow(const drogon::orm::Row& row)
{
	return UserInfoBuilder::BuildProfile(
		row["username"].as<std::string>(),
		NullableText(row, "avatar"),
		row["account"].as<std::string>(),
		row["uid"].as<std::string>(),
		NullableText(row, "signature"));
}

// ─── interface implementations ──────────────────────────────────────────────

drogon::Task<std::string> SQLiteUserRepository::GetHashedPassword(const std::string& account) const
{
	try
	{
		const auto p1 = PlaceholderFor(_db->type(), 1);
		auto result = co_await _db->execSqlCoro(
			"SELECT password FROM users WHERE account = " + p1 + " LIMIT 1",
			account);
		if (result.empty())
			co_return std::string{};
		co_return result[0]["password"].as<std::string>();
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
		const auto p1 = PlaceholderFor(_db->type(), 1);
		auto result = co_await _db->execSqlCoro(
			"SELECT uid, username, account, avatar FROM users WHERE account = " + p1 + " LIMIT 1",
			account);
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
		const auto p1 = PlaceholderFor(_db->type(), 1);
		auto result = co_await _db->execSqlCoro(
			"SELECT username, avatar FROM users WHERE uid = " + p1 + " LIMIT 1",
			uid);
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
		const auto p1 = PlaceholderFor(_db->type(), 1);
		auto result = co_await _db->execSqlCoro(
			"SELECT username, avatar, account, uid, signature FROM users WHERE uid = " + p1 + " LIMIT 1",
			uid);
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
		const auto p1 = PlaceholderFor(_db->type(), 1);
		auto result = co_await _db->execSqlCoro(
			"SELECT username, avatar, account, uid, signature FROM users WHERE account = " + p1 + " LIMIT 1",
			account);
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
	if (!info.IsDbValid())
		co_return false;

	try
	{
		const auto p1 = PlaceholderFor(_db->type(), 1);
		const auto p2 = PlaceholderFor(_db->type(), 2);
		const auto p3 = PlaceholderFor(_db->type(), 3);
		const auto p4 = PlaceholderFor(_db->type(), 4);
		const auto p5 = PlaceholderFor(_db->type(), 5);

		if (info.getAvatar().empty())
		{
			co_await _db->execSqlCoro(
				"INSERT INTO users (username, account, password, uid) VALUES ("
				+ p1 + ", " + p2 + ", " + p3 + ", " + p4 + ")",
				info.getUsername(),
				info.getAccount(),
				info.getHashedPassword(),
				info.getUid());
		}
		else
		{
			co_await _db->execSqlCoro(
				"INSERT INTO users (username, account, password, uid, avatar) VALUES ("
				+ p1 + ", " + p2 + ", " + p3 + ", " + p4 + ", " + p5 + ")",
				info.getUsername(),
				info.getAccount(),
				info.getHashedPassword(),
				info.getUid(),
				info.getAvatar());
		}
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

	try
	{
		const auto p1 = PlaceholderFor(_db->type(), 1);
		const auto p2 = PlaceholderFor(_db->type(), 2);
		const auto p3 = PlaceholderFor(_db->type(), 3);
		const auto p4 = PlaceholderFor(_db->type(), 4);
		const std::string sql =
			"UPDATE users SET "
			"username = COALESCE(" + p1 + ", username), "
			"avatar = COALESCE(" + p2 + ", avatar), "
			"signature = COALESCE(" + p3 + ", signature) "
			"WHERE uid = " + p4;

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

