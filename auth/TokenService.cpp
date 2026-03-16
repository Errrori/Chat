#include "pch.h"
#include "TokenService.h"
#include "TokenConstants.h"
#include "SecretProvider.h"
#include "Common/User.h"
#include "jwt-cpp/jwt.h"
#include <cctype>

namespace Auth {

TokenService& TokenService::GetInstance()
{
    static TokenService instance;
    return instance;
}

std::string TokenService::Sign(const std::string& uid,
                               TokenType type,
                               unsigned ttl_seconds,
                               const std::string& jti)
{
    try
    {
        const auto& secret = SecretProvider::GetInstance().GetJwtSecret();
        if (secret.empty())
        {
            LOG_ERROR << "[TokenService] JWT secret is empty";
            return {};
        }

        auto now    = std::chrono::system_clock::now();
        auto expiry = now + std::chrono::seconds{ ttl_seconds };

        const char* typ_str = (type == TokenType::Access)
            ? AccessTokenType
            : RefreshTokenType;

        return jwt::create()
            .set_type("JWT")
            .set_algorithm("HS256")
            .set_issued_at(now)
            .set_expires_at(expiry)
            .set_id(jti)
            .set_payload_claim("uid", jwt::claim(uid))
            .set_payload_claim("typ", jwt::claim(std::string(typ_str)))
            .sign(jwt::algorithm::hs256{ secret });
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "[TokenService] Sign error: " << e.what();
        return {};
    }
}

bool TokenService::Verify(const std::string& token,
                          TokenType expected_type,
                          std::string& out_uid,
                          std::string& out_jti)
{
    try
    {
        const auto& secret = SecretProvider::GetInstance().GetJwtSecret();
        const auto  decoded = jwt::decode(token);

        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{ secret });
        verifier.verify(decoded);

        if (decoded.get_expires_at() < std::chrono::system_clock::now())
        {
            LOG_WARN << "[TokenService] Token expired";
            return false;
        }

        const char* expected_typ = (expected_type == TokenType::Access)
            ? AccessTokenType
            : RefreshTokenType;

        if (!decoded.has_payload_claim("typ") ||
            decoded.get_payload_claim("typ").as_string() != expected_typ)
        {
            LOG_WARN << "[TokenService] Token type mismatch, expected: " << expected_typ;
            return false;
        }

        out_jti = decoded.has_id() ? decoded.get_id() : "";
        if (!decoded.has_payload_claim("uid"))
        {
            LOG_WARN << "[TokenService] Token missing uid claim";
            return false;
        }
        out_uid = decoded.get_payload_claim("uid").as_string();
        if (out_uid.empty())
        {
            LOG_WARN << "[TokenService] Token uid claim is empty";
            return false;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "[TokenService] Verify error: " << e.what();
        return false;
    }
}

std::string TokenService::ExtractFromRequest(const drogon::HttpRequestPtr& req)
{
    // 1. Authorization header: Bearer / JWT / raw
    std::string auth = req->getHeader("Authorization");
    if (!auth.empty())
    {
        // trim
        auto b = auth.find_first_not_of(" \t\r\n");
        auto e = auth.find_last_not_of(" \t\r\n");
        if (b != std::string::npos)
            auth = auth.substr(b, e - b + 1);

        auto startsNoCase = [](const std::string& s, const char* prefix, size_t plen) {
            if (s.size() < plen) return false;
            for (size_t i = 0; i < plen; ++i)
                if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)prefix[i]))
                    return false;
            return true;
        };

        if (startsNoCase(auth, "bearer ", 7)) return auth.substr(7);
        if (startsNoCase(auth, "jwt ", 4))    return auth.substr(4);
        return auth;  // 向后兼容：允许 header 直接是 token 值
    }

    // 2. JSON body: { "token": "..." }
    if (auto json = req->getJsonObject(); json && json->isMember("token"))
        return (*json)["token"].asString();

    // 3. Query parameter: ?token=...
    auto qp = req->getParameter("token");
    if (!qp.empty()) return qp;

    return {};
}

std::string TokenService::ExtractRefreshToken(const drogon::HttpRequestPtr& req)
{
    // 1. HttpOnly cookie is the primary carrier for refresh token.
    auto refresh_from_cookie = req->getCookie("refresh_token");
    if (!refresh_from_cookie.empty())
        return refresh_from_cookie;

    // 2. Backward compatibility: JSON body { "refresh_token": "..." }
    if (auto json = req->getJsonObject(); json && json->isMember("refresh_token"))
        return (*json)["refresh_token"].asString();

    // 3. Legacy fallback: Authorization/body(token)/query(token)
    return ExtractFromRequest(req);
}

} // namespace Auth
