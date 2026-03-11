#include "pch.h"
#include "Authentication.h"

#include "Common/User.h"
#include "Container.h"
#include "Service/UserService.h"
#include "auth/TokenService.h"

namespace
{
    // Parse authentication request body (support JSON body or query parameters)
    Json::Value ParseAuthBody(const drogon::HttpRequestPtr& req)
    {
        // First try to parse JSON body
        if (const auto& jsonBody = req->getJsonObject(); jsonBody)
        {
            return *jsonBody;
        }

        // Fallback to parse raw body as JSON
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

        // Final fallback: extract from query parameters
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
} // anonymous namespace

// Handle user registration request
drogon::Task<drogon::HttpResponsePtr> AuthController::OnRegister(drogon::HttpRequestPtr req)
{
    const auto req_body = ParseAuthBody(req);
    if (req_body.empty())
        co_return Utils::CreateErrorResponse(400, 400, "request body is not valid");

    auto user_info = UserInfo::FromJson(req_body);
    user_info.setUid(Utils::Authentication::GenerateUid());

    co_return co_await Container::GetInstance().GetUserService()->UserRegister(user_info);
}

// Handle user login request
drogon::Task<drogon::HttpResponsePtr> AuthController::OnLogin(drogon::HttpRequestPtr req)
{
    const auto req_body = ParseAuthBody(req);
    if (req_body.empty())
        co_return Utils::CreateErrorResponse(400, 400, "can not login");

    const auto info = UserInfo::FromJson(req_body);

    co_return co_await Container::GetInstance().GetUserService()->UserLogin(info);
}

// Handle token refresh request
drogon::Task<drogon::HttpResponsePtr> AuthController::OnRefresh(drogon::HttpRequestPtr req)
{
    auto refresh_token = Auth::TokenService::GetInstance().ExtractRefreshToken(req);

    if (refresh_token.empty())
        co_return Utils::CreateErrorResponse(400, 400, "refresh_token is required");

    co_return co_await Container::GetInstance().GetUserService()->RefreshToken(refresh_token);
}

// Handle user logout request
drogon::Task<drogon::HttpResponsePtr> AuthController::OnLogout(drogon::HttpRequestPtr req)
{
    auto refresh_token = Auth::TokenService::GetInstance().ExtractRefreshToken(req);

    if (refresh_token.empty())
        co_return Utils::CreateErrorResponse(400, 400, "refresh_token is required");

    co_return co_await Container::GetInstance().GetUserService()->Logout(refresh_token);
}