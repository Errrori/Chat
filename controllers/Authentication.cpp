#include "pch.h"
#include "Authentication.h"

#include "Common/User.h"
#include "Container.h"
#include "Service/UserService.h"

void Authentication::AuthController::OnRegister(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	const auto& req_body = req->getJsonObject();

	if (req_body)
	{
		auto user_info = UserInfo::FromJson(*req_body);
		user_info.setUid(Utils::Authentication::GenerateUid());
		Container::GetInstance().GetService<UserService>()->UserRegister(user_info, std::move(callback));
		return;
	}

	callback(Utils::CreateErrorResponse(400, 400, "request body is not valid"));

}

void Authentication::AuthController::OnLogin(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	const auto& req_body = req->getJsonObject();
	if (req_body)
	{
		auto info = UserInfo::FromJson(*req_body);
		Container::GetInstance().GetService<UserService>()->UserLogin(info, std::move(callback));
		return;
	}
	callback(Utils::CreateErrorResponse(400, 400, "can not login"));

}

//void Authentication::AuthController::HandleRegister(const drogon::HttpRequestPtr& req,
//                                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
//{
//	LOG_TRACE << "access the Register route req_body is : "<<req->bodyData();
//
//	auto& req_body = req->getJsonObject();
//
//	if (req_body) {
//		UsersInfo info = UsersInfo::FromJson(req_body.get());
//
//		const auto& username = info.getUsername();
//		const auto& password = info.getHashedPassword();
//		const auto& account = info.getAccount();
//
//		if (username.empty()||
//			password.empty()||
//			account.empty())
//		{
//			auto resp = Utils::CreateErrorResponse(400, 400, "Username,Account or password is empty");
//			callback(resp);
//			return;
//		}
//		if (!Utils::Authentication::IsValidAccount(account))
//		{
//			auto resp = Utils::CreateErrorResponse(400, 400, "account is not valid");
//			callback(resp);
//			return;
//		}
//
//		info.setUid(Utils::Authentication::GenerateUid());
//		
//		if ()
//		{
//			auto resp = Utils::CreateErrorResponse(500, 501, "Server database error: cannot add user");
//			callback(resp);
//			return;
//		}
//		Json::Value json_resp;
//		json_resp["uid"] = uid;
//		json_resp["code"] = 200;
//		auto resp = drogon::HttpResponse::newHttpJsonResponse(json_resp);
//		resp->addHeader("content-type", "application/json");
//		callback(resp);
//	}
//	else
//	{
//		auto resp = Utils::CreateErrorResponse(400, 400, "Request body is empty");
//		callback(resp);
//	}
//}
//
//void Authentication::AuthController::HandleLogin(const drogon::HttpRequestPtr& req,
//	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
//{
//
//	std::string account;
//	std::string password;
//	auto req_body = req->getJsonObject();
//	if (req_body) {
//		account = (*req_body)["account"].asString();
//		password = (*req_body)["password"].asString();
//		if (account.empty()) LOG_ERROR << "account is empty!\n";
//		if (password.empty()) LOG_ERROR << "password is empty!\n";
//	}
//	password = Utils::Authentication::PasswordHashed(password);
//
//	Json::Value s_data;
//	if (!DatabaseManager::GetUserInfoByAccount(account, s_data))
//	{
//		LOG_ERROR << "cam not find user info\n";
//		auto resp = Utils::CreateErrorResponse(404, 404, "User not found");
//		callback(resp);
//		return;
//	}
//
//	auto s_account = s_data["account"].asString();
//	auto s_password = s_data["password"].asString();
//	auto uid = s_data["uid"].asString();
//	auto username = s_data["username"].asString();
//
//	if (s_account!=account || s_password!=password )
//	{
//		std::string errorMsg = "Account or password is incorrect";
//		if (s_account!=account)
//			errorMsg = "account is incorrect";
//		if (s_password!=password)
//			errorMsg = "Password is incorrect";
//		
//		auto resp = Utils::CreateErrorResponse(401, 401, errorMsg);
//		LOG_ERROR << "error to verify\n";
//		callback(resp);
//		return;
//	}
//
//	Json::Value send_data;
//	send_data["message"] = "Login success";
//	send_data["code"] = 200;
//	send_data["uid"] = uid;
//
//	UserInfo info;
//	info.setAvatar()
//	auto token = Utils::Authentication::GenerateJWT(info);
//	send_data["token"] = token;
//
//	auto resp = drogon::HttpResponse::newHttpJsonResponse(send_data);
//	resp->addHeader("content-type", "application/json");
//	callback(resp);
//}

