#include "pch.h"
#include "UserService.h"
#include "Common/User.h"
#include "Data/IUserRepository.h"
#include "RedisService.h"
#include "models/Users.h"

using namespace Utils::Authentication;

drogon::Task<drogon::HttpResponsePtr> UserService::UserRegister(const UserInfo& info) const
{
	const auto& username = info.getUsername();
	const auto& password = info.getHashedPassword();
	const auto& account  = info.getAccount();

	if (username.empty() || password.empty() || account.empty() || !IsValidAccount(account))
	{
		LOG_ERROR << "UserRegister: lack of essential fields";
		co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");
	}

	try
	{
		auto record = co_await _user_repo->GetUserInfoByAccountCoro(account);
		if (record != Json::nullValue)
			co_return Utils::CreateSuccessResp(400, 400, "user is already existed");

		bool success = co_await _user_repo->AddUserCoro(info);
		if (success)
			co_return Utils::CreateSuccessResp(200, 200, "user register: " + username);
		else
			co_return Utils::CreateErrorResponse(500, 500, "can not create user info");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "UserRegister exception: " << e.what();
		co_return Utils::CreateErrorResponse(500, 500, "server error");
	}
}

drogon::Task<drogon::HttpResponsePtr> UserService::UserLogin(const UserInfo& info) const
{
	if (info.getAccount().empty() || info.getHashedPassword().empty())
		co_return Utils::CreateErrorResponse(400, 400, "lack of essential fields");

	try
	{
		auto record = co_await _user_repo->GetUserInfoByAccountCoro(info.getAccount());
		if (record == Json::nullValue)
			co_return Utils::CreateErrorResponse(400, 400, "user is not existed");

		const auto  user_record     = UserInfo::FromJson(record);
		const auto& stored_password = record["password"].asString();

		if (stored_password != info.getHashedPassword())
			co_return Utils::CreateErrorResponse(400, 400, "password is not correct");


		Json::Value data;
		data["uid"]   = user_record.getUid();
		data["token"] = GenerateJWT(user_record);

		co_await _redis_service->CacheUserInfo(user_record.getUid(), user_record.ToJson());

		co_return Utils::CreateSuccessJsonResp(200, 200, "success login", data);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "UserLogin exception: " << e.what();
		co_return Utils::CreateErrorResponse(500, 500, "server error");
	}
}

drogon::Task<UserInfo> UserService::GetUserInfo(const std::string& uid)
{
	co_return co_await _user_repo->GetUserInfo(uid);
}

drogon::Task<drogon::HttpResponsePtr> UserService::GetUser(
	std::optional<std::string> uid, std::optional<std::string> account) const
{
	const bool hasUid = uid.has_value() && !uid->empty();
	const bool hasAccount = account.has_value() && !account->empty();

	if (!hasUid && !hasAccount)
		co_return Utils::CreateErrorResponse(400, 400, "User ID or account can not be empty");

	if (hasUid && hasAccount)
		co_return Utils::CreateErrorResponse(400, 400, "can not query user in two parameters");

	try
	{
		Json::Value record;
		if (hasUid)
			record = co_await _user_repo->GetUserInfoByUidCoro(*uid);
		else
			record = co_await _user_repo->GetUserInfoByAccountCoro(*account);

		if (record == Json::nullValue)
			co_return Utils::CreateErrorResponse(404, 404, "User is not found");

		drogon_model::sqlite3::Users user(record);
		static const std::vector<std::string> mask = {
			"",             // id
			"username",
			"account",
			"",             // password
			"uid",
			"avatar",
			"create_time",
			"signature",
			"last_login_time",
			"posts",
			"level",
			"status",
			"email",
			"followers",
			"following"
		};
		Json::Value userInfo = user.toMasqueradedJson(mask);

		Json::Value response;
		response["code"] = 200;
		response["message"] = "success to get user info";
		response["data"] = userInfo;

		auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
		resp->setStatusCode(drogon::k200OK);
		co_return resp;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception in GetUser: " << e.what();
		co_return Utils::CreateErrorResponse(500, 500, "server error");
	}
}

drogon::Task<drogon::HttpResponsePtr> UserService::ModifyUserInfo(const Json::Value& body) const
{
	if (!body.isMember("account") && !body.isMember("uid"))
		co_return Utils::CreateErrorResponse(400, 400, "Invalid request data");

	const bool hasUsername  = body.isMember("username")  && !body["username"].isNull();
	const bool hasAvatar   = body.isMember("avatar")     && !body["avatar"].isNull();
	const bool hasEmail    = body.isMember("email")      && !body["email"].isNull();
	const bool hasSignature = body.isMember("signature") && !body["signature"].isNull();

	if (!hasUsername && !hasAvatar && !hasEmail && !hasSignature)
		co_return Utils::CreateErrorResponse(400, 400, "fail to modify user's info");

	try
	{
		const char* kSqlUpdateByUid =
			"UPDATE users SET "
			"username = COALESCE(?, username), "
			"avatar = COALESCE(?, avatar), "
			"email = COALESCE(?, email), "
			"signature = COALESCE(?, signature) "
			"WHERE uid = ?";

		const char* kSqlUpdateByAccount =
			"UPDATE users SET "
			"username = COALESCE(?, username), "
			"avatar = COALESCE(?, avatar), "
			"email = COALESCE(?, email), "
			"signature = COALESCE(?, signature) "
			"WHERE account = ?";

		bool ok = false;

		if (body.isMember("uid"))
		{
			const auto uid = body["uid"].asString();
			auto record = co_await _user_repo->GetUserInfoByUidCoro(uid);
			if (record == Json::nullValue)
				co_return Utils::CreateErrorResponse(404, 404, "user not found");

			// Use the existing repo's DB client through a coroutine query
			auto r = co_await drogon::app().getDbClient()->execSqlCoro(
				kSqlUpdateByUid,
				hasUsername  ? body["username"].asString()  : nullptr,
				hasAvatar   ? body["avatar"].asString()    : nullptr,
				hasEmail    ? body["email"].asString()      : nullptr,
				hasSignature ? body["signature"].asString() : nullptr,
				uid
			);
			ok = (r.affectedRows() > 0);

			if (ok)
				co_await _redis_service->InvalidateUserInfo(uid);
		}
		else
		{
			const auto account = body["account"].asString();
			auto r = co_await drogon::app().getDbClient()->execSqlCoro(
				kSqlUpdateByAccount,
				hasUsername  ? body["username"].asString()  : nullptr,
				hasAvatar   ? body["avatar"].asString()    : nullptr,
				hasEmail    ? body["email"].asString()      : nullptr,
				hasSignature ? body["signature"].asString() : nullptr,
				account
			);
			ok = (r.affectedRows() > 0);
		}

		if (!ok)
			co_return Utils::CreateErrorResponse(400, 400, "fail to modify user's info");

		Json::Value response;
		response["code"] = 200;
		response["message"] = "success to modify user's info";
		response["data"] = body;
		co_return drogon::HttpResponse::newHttpJsonResponse(response);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "ModifyUserInfo exception: " << e.what();
		co_return Utils::CreateErrorResponse(400, 400, "fail to modify user's info");
	}
}
