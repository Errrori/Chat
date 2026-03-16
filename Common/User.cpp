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

UserCachedInfo UserCachedInfo::FromJson(const Json::Value& json)
{
	UserCachedInfo profile;
	try
	{
		if (json.isMember("username"))
			profile.username = json["username"].asString();
		if (json.isMember("avatar"))
			profile.avatar = json["avatar"].asString();
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "UserCachedInfo::FromJson error: " << e.what();
		return UserCachedInfo{};
	}
	return profile;
}

bool UserCachedInfo::IsEmpty() const
{
	return username.empty() && avatar.empty();
}

Json::Value UserCachedInfo::ToJson() const
{
	Json::Value json;
	json["username"] = username;
	json["avatar"] = avatar;
	return json;
}

Json::Value UserSearchCard::ToJson() const
{
	Json::Value json;
	json["uid"] = uid;
	json["username"] = username;
	json["avatar"] = avatar;
	json["signature"] = signature;
	return json;
}

Json::Value UserProfile::ToJson() const
{
	Json::Value json;
	json["uid"] = uid;
	json["account"] = account;
	json["username"] = username;
	json["avatar"] = avatar;
	json["signature"] = signature;
	return json;
}

UserProfilePatch UserProfilePatch::FromJson(const Json::Value& json)
{
	UserProfilePatch patch;
	if (json.isMember("username") && !json["username"].isNull())
		patch.username = json["username"].asString();
	if (json.isMember("avatar") && !json["avatar"].isNull())
		patch.avatar = json["avatar"].asString();
	if (json.isMember("signature") && !json["signature"].isNull())
		patch.signature = json["signature"].asString();
	return patch;
}

bool UserProfilePatch::HasUpdates() const
{
	return username.has_value() || avatar.has_value() || signature.has_value();
}

bool UserProfilePatch::AffectsDisplayProfile() const
{
	return username.has_value() || avatar.has_value();
}

Json::Value UserProfilePatch::ToJson() const
{
	Json::Value json(Json::objectValue);
	if (username.has_value())
		json["username"] = *username;
	if (avatar.has_value())
		json["avatar"] = *avatar;
	if (signature.has_value())
		json["signature"] = *signature;
	return json;
}


UsersInfo UserInfoBuilder::BuildSignInInfo(const std::string& account, const std::string& hashed_password)
{
	UsersInfo info;
	info.SetType(UsersInfo::SignIn);
	info.SetAccount(account);
	info.SetHashedPassword(hashed_password);
	return info;
}

UsersInfo UserInfoBuilder::BuildSignInInfo(const Json::Value& json_data)
{
	try
	{
		return BuildSignInInfo(json_data["account"].asString(), json_data["password"].asString());
	}
	catch (const std::exception& e)
	{
		LOG_WARN << "The Sign-in Json data is not valid";
		return {};
	}
}

UsersInfo UserInfoBuilder::BuildSignInInfo(std::string&& account, std::string&& hashed_password)
{
	UsersInfo info;
	info.SetType(UsersInfo::SignIn);
	info.SetAccount(std::move(account));
	info.SetHashedPassword(std::move(hashed_password));
	return info;
}

UsersInfo UserInfoBuilder::BuildSignUpInfo(const std::string& username, const std::string& account,
                                           const std::string& hashed_password)
{
	UsersInfo info;
	info.SetType(UsersInfo::SignUp);
	info.SetUsername(username);
	info.SetAccount(account);
	info.SetHashedPassword(hashed_password);
	return info;
}

UsersInfo UserInfoBuilder::BuildSignUpInfo(const Json::Value& json_data)
{
	try
	{
		return BuildSignUpInfo(json_data["username"].asString(), json_data["account"].asString(),
			json_data["password"].asString());
	}
	catch (const std::exception& e)
	{
		LOG_WARN << "The Sign-up Json data is not valid";
		return {};
	}
}

UsersInfo UserInfoBuilder::BuildSignUpInfo(std::string&& username, std::string&& account, std::string&& hashed_password)
{
	UsersInfo info;
	info.SetType(UsersInfo::SignUp);
	info.SetUsername(std::move(username));
	info.SetAccount(std::move(account));
	info.SetHashedPassword(std::move(hashed_password));
	return info;
}

UsersInfo UserInfoBuilder::BuildProfile(const std::string& username, const std::string& avatar,
                                        const std::string& account, const std::string& uid, const std::string& signature)
{
	UsersInfo info;
	info.SetType(UsersInfo::Profile);
	info.SetUsername(username);
	info.SetAvatar(avatar);
	info.SetAccount(account);
	info.SetUid(uid);
	info.SetSignature(signature);
	return info;
}

UsersInfo UserInfoBuilder::BuildProfile(const Json::Value& json_data)
{
	try
	{
		return BuildProfile(json_data["username"].asString(), json_data["avatar"].asString(),
			json_data["account"].asString(), json_data["uid"].asString(), json_data["signature"].asString());
	}catch (const std::exception& e)
	{
		LOG_WARN << "The Profile Json data is not valid";
		return {};
	}
}

UsersInfo UserInfoBuilder::BuildProfile(std::string&& username, std::string&& avatar, std::string&& account,
                                        std::string&& uid, std::string&& signature)
{
	UsersInfo info;
	info.SetType(UsersInfo::Profile);
	info.SetUsername(std::move(username));
	info.SetAvatar(std::move(avatar));
	info.SetAccount(std::move(account));
	info.SetUid(std::move(uid));
	info.SetSignature(std::move(signature));
	return info;
}

UsersInfo UserInfoBuilder::BuildCached(const std::string& username, const std::string& avatar)
{
	UsersInfo info;
	info.SetType(UsersInfo::Cached);
	info.SetUsername(username);
	info.SetAvatar(avatar);
	return info;
}

UsersInfo UserInfoBuilder::BuildCached(const Json::Value& json_data)
{
	try
	{
		return BuildCached(json_data["username"].asString(), json_data["avatar"].asString());
	}
	catch (const std::exception& e)
	{
		LOG_WARN << "The Cached Json data is not valid";
		return {};
	}
}

UsersInfo UserInfoBuilder::BuildCached(std::string&& username, std::string&& avatar)
{
	UsersInfo info;
	info.SetType(UsersInfo::Cached);
	info.SetUsername(std::move(username));
	info.SetAvatar(std::move(avatar));
	return info;
}

UsersInfo UserInfoBuilder::BuildUpdatedInfo(std::optional<std::string> username, std::optional<std::string> avatar,
	std::optional<std::string> signature)
{
	UsersInfo info;
	info.SetType(UsersInfo::UpdateInfo);
	if (username) 
		info.SetUsername(std::move(*username));
	if (avatar)
		info.SetAvatar(std::move(*avatar));
	if (signature)
		info.SetSignature(std::move(*signature));
	return info;
}

UsersInfo UserInfoBuilder::BuildUpdatedInfo(const Json::Value& json_data)
{
	try
	{
		UsersInfo info;
		if (json_data.isMember("username")&&!json_data["username"].isNull())
			info.SetUsername(json_data["username"].asString());
		if (json_data.isMember("avatar") && !json_data["avatar"].isNull())
			info.SetAvatar(json_data["avatar"].asString());
		if (json_data.isMember("signature")&&!json_data["signature"].isNull())
			info.SetSignature(json_data["signature"].asString());
		info.SetType(UsersInfo::UpdateInfo);
		return info;
	}
	catch (const std::exception& e)
	{
		LOG_WARN << "The UpdatedInfo Json data is not valid";
		return {};
	}
}


bool UsersInfo::IsValid() const
{
	switch (type_)
	{
		case SignIn:
			return account_.has_value() && hashed_password_.has_value();
		case SignUp:
			return username_.has_value() && account_.has_value() && hashed_password_.has_value();
		case Profile:
			return username_.has_value() && avatar_.has_value()
				&& account_.has_value() && uid_.has_value() && signature_.has_value();
		case Cached:
			return username_.has_value() && avatar_.has_value();
		case UpdateInfo:
			return username_.has_value() || avatar_.has_value() || signature_.has_value();
		default:
			return false;
	}
}

bool UsersInfo::HasUpdates() const
{
	if (type_ != UpdateInfo)
		return false;
	return username_.has_value() || avatar_.has_value() || signature_.has_value();
}

bool UsersInfo::AffectsDisplayProfile() const
{
	if (type_ != UpdateInfo)
		return false;
	return username_.has_value() || avatar_.has_value();
}

Json::Value UsersInfo::ToJson() const
{
	//important: never serialize hashed_password!!!

	if (!IsValid())
		return Json::nullValue;
	Json::Value data;
	if (username_.has_value())
		data["username"] = username_.value();
	if (avatar_.has_value())
		data["avatar"] = avatar_.value();
	if (signature_.has_value())
		data["signature"] = signature_.value();
	if (account_.has_value())
		data["account"] = account_.value();
	if (uid_.has_value())
		data["uid"] = uid_.value();
	return data;
		
}


