#pragma once
#include <string>
#include <memory>

class UserInfo;
namespace drogon { class HttpRequest; using HttpRequestPtr = std::shared_ptr<HttpRequest>; }

namespace Auth {

enum class TokenType { Access, Refresh };

/**
 * TokenService - Low-level JWT signing and verification.
 *
 * Access Token:  Contains uid/account/username/avatar/typ=access/jti/exp
 * Refresh Token: Contains uid/jti/typ=refresh/exp (minimal claims, no business data)
 *
 * typ field ensures refresh token cannot be used as access token.
 * jti field is used for refresh token rotation/revocation tracking.
 */
class TokenService
{
public:
    static TokenService& GetInstance();

    /**
     * Sign a JWT.
     * @param uid          User uid
     * @param account      User account (empty string for refresh token)
     * @param username     Username (empty string for refresh token)
     * @param avatar       Avatar (empty string for refresh token)
     * @param type         TokenType::Access or Refresh
     * @param ttl_seconds  TTL in seconds
     * @param jti          JWT ID (passed by caller, used to track refresh session in Redis)
     * @return  JWT string; empty string on failure
     */
    std::string Sign(const std::string& uid,
                     const std::string& account,
                     const std::string& username,
                     const std::string& avatar,
                     TokenType type,
                     unsigned ttl_seconds,
                     const std::string& jti);

    /**
     * Verify JWT and populate out_info.
     * @param token         JWT string
     * @param expected_type Expected token type; returns false immediately if type mismatch
     * @param out_info      User info populated on success
     * @param out_jti       jti populated on success (needed for refresh verification)
     * @return true = valid, false = invalid/expired/type mismatch
     */
    bool Verify(const std::string& token,
                TokenType expected_type,
                UserInfo& out_info,
                std::string& out_jti);

    /// Extract token string from HTTP request (supports Bearer header / JWT header / query param)
    std::string ExtractFromRequest(const drogon::HttpRequestPtr& req);

    /// Extract refresh token from request (cookie first, then body/header/query)
    std::string ExtractRefreshToken(const drogon::HttpRequestPtr& req);

private:
    TokenService() = default;
    TokenService(const TokenService&) = delete;
    TokenService& operator=(const TokenService&) = delete;
};

} // namespace Auth
