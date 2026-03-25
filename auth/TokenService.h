#pragma once
#include <string>
#include <memory>

namespace drogon { class HttpRequest; using HttpRequestPtr = std::shared_ptr<HttpRequest>; }

namespace Auth {

	enum class TokenType { Access, Refresh };

	struct TokenData
    {
		std::string uid;
        std::string jti;
		std::chrono::time_point<std::chrono::system_clock> expire_at;
    };
/**
 * TokenService - Low-level JWT signing and verification.
 *
 * Both access and refresh tokens carry only: uid / typ / jti / exp.
 * All user-profile data (username, avatar, account) is resolved from
 * Redis cache or DB after verification — never embedded in the token.
 */
class TokenService
{
public:
    static TokenService& GetInstance();

    /// Sign a JWT containing uid + typ + jti + exp.
    std::string Sign(const std::string& uid,
                     TokenType type,
                     unsigned ttl_seconds,
                     const std::string& jti);

    /// Verify JWT; on success populates out_uid and out_jti.
    std::optional<TokenData> Verify(const std::string& token,
                                    TokenType expected_type);

    /// Extract access token from HTTP request (Bearer header / JWT header / query param).
    std::string ExtractFromRequest(const drogon::HttpRequestPtr& req);

    /// Extract refresh token from request (cookie first, then body/header/query).
    std::string ExtractRefreshToken(const drogon::HttpRequestPtr& req);

private:
    TokenService() = default;
    TokenService(const TokenService&) = delete;
    TokenService& operator=(const TokenService&) = delete;
};

} // namespace Auth
