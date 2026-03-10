#include "pch.h"
#include "User.h"
#include "models/Users.h"

UserInfo UserInfo::FromJson(const Json::Value& json)
{
	UserInfo info{};
	auto readIntField = [](const Json::Value& src, const char* key, int& out)
	{
		if (!src.isMember(key) || src[key].isNull())
			return;

		const auto& value = src[key];
		if (value.isInt())
		{
			out = value.asInt();
			return;
		}
		if (value.isInt64() || value.isUInt() || value.isUInt64())
		{
			out = static_cast<int>(value.asInt64());
			return;
		}
		if (value.isString())
		{
			try
			{
				out = std::stoi(value.asString());
			}
			catch (const std::exception&)
			{
				LOG_WARN << "invalid integer field: " << key << ", value=" << value.asString();
			}
			return;
		}

		LOG_WARN << "unexpected json type for integer field: " << key;
	};

	auto readInt64Field = [](const Json::Value& src, const char* key, int64_t& out, int64_t defaultVal = -1)
	{
		if (!src.isMember(key) || src[key].isNull())
		{
			if (defaultVal >= 0)
				out = defaultVal;
			return;
		}

		const auto& value = src[key];
		if (value.isInt64() || value.isInt() || value.isUInt() || value.isUInt64())
		{
			out = value.asInt64();
			return;
		}
		if (value.isString())
		{
			try
			{
				out = std::stoll(value.asString());
			}
			catch (const std::exception&)
			{
				LOG_WARN << "invalid int64 field: " << key << ", value=" << value.asString();
				if (defaultVal >= 0)
					out = defaultVal;
			}
			return;
		}

		LOG_WARN << "unexpected json type for int64 field: " << key;
		if (defaultVal >= 0)
			out = defaultVal;
	};

	try
	{
		if (json.isMember("username"))
			info.username = json["username"].asString();
		if (json.isMember("avatar"))
			info.avatar = json["avatar"].asString();
		if (json.isMember("email"))
			info.email = json["email"].asString();
		if (json.isMember("signature"))
			info.signature = json["signature"].asString();

		readIntField(json, "posts", info.posts);
		readIntField(json, "followers", info.followers);
		readIntField(json, "following", info.following);

		auto currentTime = Utils::GetCurrentTimeStamp();
		info.create_time = currentTime;
		info.last_login_time = currentTime;
		readInt64Field(json, "create_time", info.create_time, currentTime);
		readInt64Field(json, "last_login_time", info.last_login_time, currentTime);
		readIntField(json, "status", info.status);

		if (json.isMember("uid"))
			info.uid = json["uid"].asString();
		if (json.isMember("account"))
			info.account = json["account"].asString();
		if (json.isMember("password"))
			info.hashed_password = Utils::Authentication::PasswordHashed(json["password"].asString());

		readIntField(json, "level", info.level);
	}catch (const std::exception& e)
	{
		LOG_ERROR << "fail to build UsersInfo from json:"<<e.what();
		return UserInfo{};
	}

	return info;
}

std::optional<drogon_model::sqlite3::Users> UserInfo::ToDbUsers() const
{
	if (!IsDbValid())
		return std::nullopt;

	drogon_model::sqlite3::Users user;
	user.setUsername(username);
	user.setUid(uid);
	user.setAccount(account);
	user.setPassword(hashed_password);

	if (!avatar.empty())
		user.setAvatar(avatar);
	if (!email.empty())
		user.setEmail(email);
	if (!signature.empty())
		user.setSignature(signature);
	if (create_time>0)
		user.setCreateTime(create_time);
	else
		user.setCreateTime(Utils::GetCurrentTimeStamp());
	if (last_login_time>0)
		user.setLastLoginTime(last_login_time);
	else
		user.setLastLoginTime(Utils::GetCurrentTimeStamp());

	if (posts>0)
		user.setPosts(static_cast<int64_t>(posts));
	if (followers>0)
		user.setFollowers(static_cast<int64_t>(followers));
	if (following>0)
		user.setFollowing(static_cast<int64_t>(following));
	if (level>0)
		user.setLevel(static_cast<int64_t>(level));
	if (status>0)
		user.setStatus(static_cast<int64_t>(status));

	return user;
}

bool UserInfo::IsDbValid() const
{
	return !username.empty() && !uid.empty() && !account.empty() && !hashed_password.empty();
}

std::string UserInfo::ToString() const
{
	return "UsersInfo{username: " + username + 
		", uid: " + uid + 
		", account: " + account + 
		", avatar: " + avatar + 
		", email: " + email + 
		", signature: " + signature + 
		", status: " + std::to_string(status) + 
		", posts: " + std::to_string(posts) + 
		", followers: " + std::to_string(followers) + 
		", following: " + std::to_string(following) + 
		", level: " + std::to_string(level) + 
		", create_time: " + std::to_string(create_time) + 
		", last_login_time: " + std::to_string(last_login_time) + "}";
}

Json::Value UserInfo::ToJson() const
{
	Json::Value json;
	json["username"] = username;
	json["uid"] = uid;
	json["account"] = account;
	json["avatar"] = avatar;
	json["email"] = email;
	json["signature"] = signature;
	json["status"] = status;
	if (posts != DefaultVal) {
		json["posts"] = posts;
	}
	if (followers != DefaultVal) {
		json["followers"] = followers;
	}
	if (following != DefaultVal) {
		json["following"] = following;
	}

	if (level != DefaultVal) {
		json["level"] = level;
	}

	if (create_time>0) {
		json["create_time"] = (Json::Value::Int64)create_time;
	}
	if (last_login_time>0) {
		json["last_login_time"] = (Json::Value::Int64)last_login_time;
	}
	return json;
}

void UserInfo::Reset()
{
	username.clear();
	uid.clear();
	account.clear();
	hashed_password.clear();
	avatar.clear();
	email.clear();
	posts = DefaultVal;
	followers = DefaultVal;
	following = DefaultVal;
	level = DefaultVal;
	status = 0;
	create_time = -1;
	last_login_time = -1;
	signature.clear();
}