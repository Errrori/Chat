#pragma once
#include <string>

namespace Auth {

struct AccessToken
{
    std::string value;
    unsigned    ttl;
};

struct RefreshToken
{
    std::string value;
    std::string jti;
    unsigned    ttl;
};

struct TokenPair
{
    AccessToken  access;
    RefreshToken refresh;
};

/**
 * TokenFactory — high-level token issuance.
 * All tokens are uid-only; profile data is resolved from cache/DB after verify.
 */
class TokenFactory
{
public:
    /// Login: generate access + refresh pair from uid.
    static TokenPair    GeneratePair(const std::string& uid);

    /// Refresh: generate new access token by uid
    static AccessToken  GenerateAccess(const std::string& uid);

    /// Refresh: generate new refresh token (full rotation)
    static RefreshToken GenerateRefresh(const std::string& uid);

private:
    TokenFactory() = delete;
};

} // namespace Auth
