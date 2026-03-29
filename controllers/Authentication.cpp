#include "pch.h"
#include "Common/ResponseHelper.h"
#include "Authentication.h"

#include "Common/User.h"
#include "Container.h"
#include "Service/UserService.h"
#include "auth/TokenService.h"
#include "Utils.h"

// Handle user registration request
drogon::Task<drogon::HttpResponsePtr> AuthController::OnRegister(drogon::HttpRequestPtr req)
{
    try
    {
        auto json_body = req->getJsonObject();
        Json::Value body;
        if (json_body)
        {
            body = *json_body;
            if (!body.isMember("username") || !body.isMember("account") || !body.isMember("password"))
                co_return ResponseHelper::MakeResponse(400, 400, "missing required fields: username, account, password");
        }
        else
            co_return ResponseHelper::MakeResponse(400, 400, "request body is not valid JSON");

        auto user_info = UserInfoBuilder::BuildSignUpInfo(
            body["username"].asString(), body["account"].asString(),
            Utils::Authentication::PasswordHashed(body["password"].asString()));

        user_info.SetUid(Utils::Authentication::GenerateUid());

        auto resp = co_await Container::GetInstance().GetUserService()->UserRegister(user_info);
        co_return resp;
    }catch (const std::exception& e)
    {
        LOG_ERROR << e.what();
		co_return ResponseHelper::MakeResponse(500, 500, "server error");
    }
	
}

// Handle user login request
drogon::Task<drogon::HttpResponsePtr> AuthController::OnLogin(drogon::HttpRequestPtr req)
{
    auto json_body = req->getJsonObject();
    Json::Value body;
    if (json_body)
    {
        body = *json_body;
        if (!body.isMember("account") || !body.isMember("password"))
            co_return ResponseHelper::MakeResponse(400, 400, "missing required fields: account, password");
    }
    else
        co_return ResponseHelper::MakeResponse(400, 400, "request body is not valid JSON");
	auto info = UserInfoBuilder::BuildSignInInfo(
		body["account"].asString(),
		Utils::Authentication::PasswordHashed(body["password"].asString()));

    auto resp = co_await Container::GetInstance().GetUserService()->UserLogin(info);
    co_return resp;
}

// Handle token refresh request
drogon::Task<drogon::HttpResponsePtr> AuthController::OnRefresh(drogon::HttpRequestPtr req)
{

    auto refresh_token = Auth::TokenService::GetInstance().ExtractRefreshToken(req);

    if (refresh_token.empty())
        co_return ResponseHelper::MakeResponse(400, 400, "refresh_token is required");

    auto resp = co_await Container::GetInstance().GetUserService()->RefreshToken(refresh_token);
    co_return resp;
}

// Handle user logout request
drogon::Task<drogon::HttpResponsePtr> AuthController::OnLogout(drogon::HttpRequestPtr req)
{
    auto refresh_token = Auth::TokenService::GetInstance().ExtractRefreshToken(req);

    if (refresh_token.empty())
        co_return ResponseHelper::MakeResponse(400, 400, "refresh_token is required");

    auto resp = co_await Container::GetInstance().GetUserService()->Logout(refresh_token);
    co_return resp;
}