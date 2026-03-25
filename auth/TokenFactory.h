#pragma once
#include <string>

namespace Auth {
	enum class TokenType;

	struct Token
{
    std::string value;
    std::string jti;
};


/**
 * TokenFactory — high-level token issuance.
 * All tokens are uid-only; profile data is resolved from cache/DB after verify.
 */
class TokenFactory
{
public:
	/// Refresh: generate new access token by uid
    static Token  GenerateToken(const std::string& uid, TokenType type);

    TokenFactory() = delete;
};

} // namespace Auth
