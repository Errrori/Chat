#include "pch.h"
#include "Authentication.h"

void Authentication::Controller::HandleRegister(const drogon::HttpRequestPtr& req,
                                                std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	LOG_TRACE << "access the Register route data is : "<<req->bodyData();

	auto data = req->getJsonObject();
	if (data) {
		auto account = (*data)["account"].asString();
		auto username = (*data)["username"].asString();
		auto password = (*data)["password"].asString();

		if (username.empty()||password.empty()||account.empty())
		{
			auto resp = Utils::CreateErrorResponse(400, 400, "Username,Account or password is empty");
			callback(resp);
			return;
		}
		if (!Utils::Authentication::IsValidAccount(account))
		{
			auto resp = Utils::CreateErrorResponse(400, 400, "account is not valid");
			callback(resp);
			return;
		}

		auto uid = Utils::Authentication::GenerateUid();
		auto hashed_password = Utils::Authentication::PasswordHashed(password);
		Json::Value user_info;
		user_info["account"] = account;
		user_info["username"] = username;
		user_info["password"] = hashed_password;
		user_info["uid"] = uid;

		if (!DatabaseManager::PushUser(user_info))
		{
			auto resp = Utils::CreateErrorResponse(500, 501, "Server database error: cannot add user");
			callback(resp);
			return;
		}
		Json::Value json_resp;
		json_resp["uid"] = uid;
		json_resp["code"] = 200;
		auto resp = drogon::HttpResponse::newHttpJsonResponse(json_resp);
		resp->addHeader("content-type", "application/json");
		callback(resp);
	}
	else
	{
		auto resp = Utils::CreateErrorResponse(400, 400, "Request body is empty");
		callback(resp);
	}
}

//ԺŻ
void Authentication::Controller::HandleLogin(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	LOG_TRACE << "access the Login route\n";
	LOG_TRACE << "data is : " << req->bodyData();
	std::string account;
	std::string password;
	auto data = req->getJsonObject();
	if (data) {
		account = (*data)["account"].asString();
		password = (*data)["password"].asString();
		if (account.empty()) LOG_ERROR << "account is empty!\n";
		if (password.empty()) LOG_ERROR << "password is empty!\n";
	}
	password = Utils::Authentication::PasswordHashed(password);

	Json::Value s_data;
	if (!DatabaseManager::GetUserInfoByAccount(account, s_data))
	{
		LOG_ERROR << "cam not find user info\n";
		auto resp = Utils::CreateErrorResponse(404, 404, "User not found");
		callback(resp);
		return;
	}

	auto s_account = s_data["account"].asString();
	auto s_password = s_data["password"].asString();
	auto uid = s_data["uid"].asString();
	auto username = s_data["username"].asString();

	if (s_account!=account || s_password!=password )
	{
		std::string errorMsg = "Account or password is incorrect";
		if (s_account!=account)
			errorMsg = "account is incorrect";
		if (s_password!=password)
			errorMsg = "Password is incorrect";
		
		auto resp = Utils::CreateErrorResponse(401, 401, errorMsg);
		LOG_ERROR << "error to verify\n";
		callback(resp);
		return;
	}

	Json::Value send_data;
	send_data["message"] = "Login success";
	send_data["code"] = 200;
	send_data["uid"] = uid;

	Utils::UserInfo info{account,uid,username};
	auto token = Utils::Authentication::GenerateJWT(info);
	send_data["token"] = token;

	auto resp = drogon::HttpResponse::newHttpJsonResponse(send_data);
	resp->addHeader("content-type", "application/json");
	callback(resp);
}

