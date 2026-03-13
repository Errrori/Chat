#include "pch.h"
#include "User.h"
#include "models/Users.h"

// Builds a UserInfo from a JSON object (registration body or Redis cache).
// Only reads the 4 public fields; password is hashed from "password" key if present.
UserInfo UserInfo::FromJson(const Json::Value& json)
{
	UserInfo info{};
	try
	{
		if (json.isMember("uid"))      info.uid      = json["uid"].asString();
		if (json.isMember("username")) info.username = json["username"].asString();
		if (json.isMember("account"))  info.account  = json["account"].asString();
		if (json.isMember("avatar"))   info.avatar   = json["avatar"].asString();
		if (json.isMember("password"))
			info.hashed_password = Utils::Authentication::PasswordHashed(json["password"].asString());
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "UserInfo::FromJson error: " << e.what();
		return UserInfo{};
	}
	return info;
}

std::optional<drogon_model::sqlite3::Users> UserInfo::ToDbUsers() const
{
	if (!IsDbValid())
		return std::nullopt;

	drogon_model::sqlite3::Users user;
	user.setUid(uid);
	user.setUsername(username);
	user.setAccount(account);
	user.setPassword(hashed_password);
	if (!avatar.empty())
		user.setAvatar(avatar);

	return user;
}

bool UserInfo::IsDbValid() const
{
	return !uid.empty() && !username.empty() && !account.empty() && !hashed_password.empty();
}

std::string UserInfo::ToString() const
{
	return "UserInfo{uid:" + uid + ", username:" + username +
		", account:" + account + ", avatar:" + avatar + "}";
}

Json::Value UserInfo::ToJson() const
{
	Json::Value json;
	json["uid"]      = uid;
	json["username"] = username;
	json["account"]  = account;
	json["avatar"]   = avatar;
	return json;
}

void UserInfo::Reset()
{
	uid.clear();
	username.clear();
	account.clear();
	avatar.clear();
	hashed_password.clear();
}