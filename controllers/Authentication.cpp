#include "pch.h"
#include "Authentication.h"

#include "Common/User.h"
#include "Container.h"
#include "Service/UserService.h"

void AuthController::OnRegister(const drogon::HttpRequestPtr& req,
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

void AuthController::OnLogin(const drogon::HttpRequestPtr& req,
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

