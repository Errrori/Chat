#include "pch.h"
#include "User.h"

#include "models/Users.h"

UserInfo UserInfo::FromJson(const Json::Value& json)
{
	UserInfo info{};
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
		if (json.isMember("posts"))
			info.posts = json["posts"].asInt();
		if (json.isMember("followers"))
			info.followers = json["followers"].asInt();
		if (json.isMember("following"))
			info.following = json["following"].asInt();
		if (json.isMember("create_time"))
			info.create_time = json["create_time"].asInt64();
		else
			info.create_time = Utils::GetCurrentTimeStamp();
		if (json.isMember("last_login_time"))
			info.last_login_time = json["last_login_time"].asInt64();
		else
			info.last_login_time = Utils::GetCurrentTimeStamp();
		if (json.isMember("status"))
			info.status = json["status"].asInt();
		if (json.isMember("uid"))
			info.uid = json["uid"].asString();
		if (json.isMember("account"))
			info.account = json["account"].asString();
		if (json.isMember("password"))
			info.hashed_password = Utils::Authentication::PasswordHashed(json["password"].asString());
		if (json.isMember("level"))
			info.level = json["level"].asInt();
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
	
	// 设置必填字段
	user.setUsername(username);
	user.setUid(uid);
	user.setAccount(account);
	user.setPassword(hashed_password);
	
	// 设置可选字段
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

	// 设置数值字段
	if (posts>0)
		user.setPosts(static_cast<int64_t>(posts));
	if (followers>0)
		user.setFollowers(static_cast<int64_t>(followers));
	if (following>0)
		user.setFollowing(static_cast<int64_t>(following));
	if (level>0)
		user.setLevel(static_cast<int64_t>(level));
	if (status>0)//如果后面字段的枚举值有变动，这里的逻辑也要修改
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
	
	// 只有非默认值才放入JSON
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
