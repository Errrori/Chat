#include "pch.h"
#include "User.h"
#include "models/Users.h"

UserInfo UserInfoBuilder::BuildSignInInfo(const std::string& account, const std::string& hashed_password)
{
	UserInfo info;
	info.SetType(UserInfo::SignIn);
	info.SetAccount(account);
	info.SetHashedPassword(hashed_password);
	return info;
}

UserInfo UserInfoBuilder::BuildSignInInfo(const Json::Value& json_data)
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

UserInfo UserInfoBuilder::BuildSignInInfo(std::string&& account, std::string&& hashed_password)
{
	UserInfo info;
	info.SetType(UserInfo::SignIn);
	info.SetAccount(std::move(account));
	info.SetHashedPassword(std::move(hashed_password));
	return info;
}

UserInfo UserInfoBuilder::BuildSignUpInfo(const std::string& username, const std::string& account,
                                           const std::string& hashed_password)
{
	UserInfo info;
	info.SetType(UserInfo::SignUp);
	info.SetUsername(username);
	info.SetAccount(account);
	info.SetHashedPassword(hashed_password);
	return info;
}

UserInfo UserInfoBuilder::BuildSignUpInfo(const Json::Value& json_data)
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

UserInfo UserInfoBuilder::BuildSignUpInfo(std::string&& username, std::string&& account, std::string&& hashed_password)
{
	UserInfo info;
	info.SetType(UserInfo::SignUp);
	info.SetUsername(std::move(username));
	info.SetAccount(std::move(account));
	info.SetHashedPassword(std::move(hashed_password));
	return info;
}

UserInfo UserInfoBuilder::BuildProfile(const std::string& username, const std::string& avatar,
                                        const std::string& account, const std::string& uid, const std::string& signature)
{
	UserInfo info;
	info.SetType(UserInfo::Profile);
	info.SetUsername(username);
	info.SetAvatar(avatar);
	info.SetAccount(account);
	info.SetUid(uid);
	info.SetSignature(signature);
	return info;
}

UserInfo UserInfoBuilder::BuildProfile(const Json::Value& json_data)
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

UserInfo UserInfoBuilder::BuildProfile(std::string&& username, std::string&& avatar, std::string&& account,
                                        std::string&& uid, std::string&& signature)
{
	UserInfo info;
	info.SetType(UserInfo::Profile);
	info.SetUsername(std::move(username));
	info.SetAvatar(std::move(avatar));
	info.SetAccount(std::move(account));
	info.SetUid(std::move(uid));
	info.SetSignature(std::move(signature));
	return info;
}

UserInfo UserInfoBuilder::BuildCached(const std::string& username, const std::string& avatar)
{
	UserInfo info;
	info.SetType(UserInfo::Cached);
	info.SetUsername(username);
	info.SetAvatar(avatar);
	return info;
}

UserInfo UserInfoBuilder::BuildCached(const Json::Value& json_data)
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

UserInfo UserInfoBuilder::BuildCached(std::string&& username, std::string&& avatar)
{
	UserInfo info;
	info.SetType(UserInfo::Cached);
	info.SetUsername(std::move(username));
	info.SetAvatar(std::move(avatar));
	return info;
}

UserInfo UserInfoBuilder::BuildUpdatedInfo(std::optional<std::string> username, std::optional<std::string> avatar,
	std::optional<std::string> signature)
{
	UserInfo info;
	info.SetType(UserInfo::UpdateInfo);
	if (username) 
		info.SetUsername(std::move(*username));
	if (avatar)
		info.SetAvatar(std::move(*avatar));
	if (signature)
		info.SetSignature(std::move(*signature));
	return info;
}

UserInfo UserInfoBuilder::BuildUpdatedInfo(const Json::Value& json_data)
{
	try
	{
		UserInfo info;
		if (json_data.isMember("username")&&!json_data["username"].isNull())
			info.SetUsername(json_data["username"].asString());
		if (json_data.isMember("avatar") && !json_data["avatar"].isNull())
			info.SetAvatar(json_data["avatar"].asString());
		if (json_data.isMember("signature")&&!json_data["signature"].isNull())
			info.SetSignature(json_data["signature"].asString());
		info.SetType(UserInfo::UpdateInfo);
		return info;
	}
	catch (const std::exception& e)
	{
		LOG_WARN << "The UpdatedInfo Json data is not valid";
		return {};
	}
}


bool UserInfo::IsValid() const
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

bool UserInfo::HasUpdates() const
{
	if (type_ != UpdateInfo)
		return false;
	return username_.has_value() || avatar_.has_value() || signature_.has_value();
}

bool UserInfo::AffectsDisplayProfile() const
{
	if (type_ != UpdateInfo)
		return false;
	return username_.has_value() || avatar_.has_value();
}

Json::Value UserInfo::ToJson() const
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


