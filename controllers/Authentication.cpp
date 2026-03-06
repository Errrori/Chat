#include "pch.h"
#include "Authentication.h"

#include "Common/User.h"
#include "Container.h"
#include "Service/UserService.h"

namespace
{
Json::Value ParseAuthBody(const drogon::HttpRequestPtr& req)
{
	if (const auto& jsonBody = req->getJsonObject(); jsonBody)
	{
		return *jsonBody;
	}

	const auto rawBody = req->getBody();
	if (!rawBody.empty())
	{
		Json::CharReaderBuilder builder;
		std::string errs;
		Json::Value parsed;
		std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		if (reader->parse(rawBody.data(), rawBody.data() + rawBody.size(), &parsed, &errs) && parsed.isObject())
		{
			return parsed;
		}
	}

	Json::Value fallback(Json::objectValue);
	const auto account = req->getParameter("account");
	const auto password = req->getParameter("password");
	const auto username = req->getParameter("username");
	if (!account.empty())
	{
		fallback["account"] = account;
	}
	if (!password.empty())
	{
		fallback["password"] = password;
	}
	if (!username.empty())
	{
		fallback["username"] = username;
	}
	return fallback;
}
}

void AuthController::OnRegister(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	const auto req_body = ParseAuthBody(req);

	if (!req_body.empty())
	{
		auto user_info = UserInfo::FromJson(req_body);
		user_info.setUid(Utils::Authentication::GenerateUid());
		Container::GetInstance().GetService<UserService>()->UserRegister(user_info, std::move(callback));
		return;
	}

	callback(Utils::CreateErrorResponse(400, 400, "request body is not valid"));

}

void AuthController::OnLogin(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	const auto req_body = ParseAuthBody(req);
	if (!req_body.empty())
	{
		auto info = UserInfo::FromJson(req_body);
		Container::GetInstance().GetService<UserService>()->UserLogin(info, std::move(callback));
		return;
	}
	callback(Utils::CreateErrorResponse(400, 400, "can not login"));

}

