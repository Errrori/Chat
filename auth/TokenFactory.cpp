#include "pch.h"
#include "TokenFactory.h"
#include "TokenService.h"
#include "TokenConstants.h"

namespace Auth {

// static
Token TokenFactory::GenerateToken(const std::string& uid, TokenType type)
{
    const auto jti = Utils::Authentication::GenerateUid();
    Token token;
    token.value = TokenService::GetInstance().Sign(
        uid, type, type==TokenType::Access?AccessTokenTTL:RefreshTokenTTL, jti);
	token.jti = jti;
    return token;
}

} // namespace Auth
