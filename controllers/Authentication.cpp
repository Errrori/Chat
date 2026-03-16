#include "pch.h"
#include "Authentication.h"

#include "Common/User.h"
#include "Container.h"
#include "Service/UserService.h"
#include "auth/TokenService.h"

namespace
{
    int64_t NowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    std::string ResolveClientIp(const drogon::HttpRequestPtr& req)
    {
        auto real_ip = req->getHeader("X-Real-IP");
        if (!real_ip.empty())
            return real_ip;

        auto forwarded = req->getHeader("X-Forwarded-For");
        if (!forwarded.empty())
            return forwarded;

        return "unknown";
    }

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
    const auto t0 = NowMs();
    LOG_INFO << "[AuthController] OnRegister begin ip=" << ResolveClientIp(req);

    const auto req_body = ParseAuthBody(req);
    if (req_body.empty())
    {
        LOG_WARN << "[AuthController] OnRegister invalid request body cost_ms=" << (NowMs() - t0);
        co_return Utils::CreateErrorResponse(400, 400, "request body is not valid");
    }

    auto user_info = UserInfo::FromJson(req_body);
    user_info.setUid(Utils::Authentication::GenerateUid());

    auto resp = co_await Container::GetInstance().GetUserService()->UserRegister(user_info);
    LOG_INFO << "[AuthController] OnRegister end status=" << static_cast<int>(resp->statusCode())
             << " cost_ms=" << (NowMs() - t0);
    co_return resp;
}

// Handle user login request
drogon::Task<drogon::HttpResponsePtr> AuthController::OnLogin(drogon::HttpRequestPtr req)
{
    const auto t0 = NowMs();
    LOG_INFO << "[AuthController] OnLogin begin ip=" << ResolveClientIp(req);

    const auto req_body = ParseAuthBody(req);
    if (req_body.empty())
    {
        LOG_WARN << "[AuthController] OnLogin invalid request body cost_ms=" << (NowMs() - t0);
        co_return Utils::CreateErrorResponse(400, 400, "can not login");
    }

    const auto info = UserInfo::FromJson(req_body);

    auto resp = co_await Container::GetInstance().GetUserService()->UserLogin(info);
    LOG_INFO << "[AuthController] OnLogin end status=" << static_cast<int>(resp->statusCode())
             << " cost_ms=" << (NowMs() - t0);
    co_return resp;
}

// Handle token refresh request
drogon::Task<drogon::HttpResponsePtr> AuthController::OnRefresh(drogon::HttpRequestPtr req)
{
    const auto t0 = NowMs();
    LOG_INFO << "[AuthController] OnRefresh begin ip=" << ResolveClientIp(req);

    auto refresh_token = Auth::TokenService::GetInstance().ExtractRefreshToken(req);

    if (refresh_token.empty())
    {
        LOG_WARN << "[AuthController] OnRefresh missing refresh token cost_ms=" << (NowMs() - t0);
        co_return Utils::CreateErrorResponse(400, 400, "refresh_token is required");
    }

    auto resp = co_await Container::GetInstance().GetUserService()->RefreshToken(refresh_token);
    LOG_INFO << "[AuthController] OnRefresh end status=" << static_cast<int>(resp->statusCode())
             << " cost_ms=" << (NowMs() - t0);
    co_return resp;
}

// Handle user logout request
drogon::Task<drogon::HttpResponsePtr> AuthController::OnLogout(drogon::HttpRequestPtr req)
{
    const auto t0 = NowMs();
    LOG_INFO << "[AuthController] OnLogout begin ip=" << ResolveClientIp(req);

    auto refresh_token = Auth::TokenService::GetInstance().ExtractRefreshToken(req);

    if (refresh_token.empty())
    {
        LOG_WARN << "[AuthController] OnLogout missing refresh token cost_ms=" << (NowMs() - t0);
        co_return Utils::CreateErrorResponse(400, 400, "refresh_token is required");
    }

    auto resp = co_await Container::GetInstance().GetUserService()->Logout(refresh_token);
    LOG_INFO << "[AuthController] OnLogout end status=" << static_cast<int>(resp->statusCode())
             << " cost_ms=" << (NowMs() - t0);
    co_return resp;
}