#include "pch.h"
#include "TokenFactory.h"
#include "TokenService.h"
#include "TokenConstants.h"
#include "Common/User.h"

// 借用 Utils::Authentication::GenerateUid() 生成 jti
namespace Utils { namespace Authentication { std::string GenerateUid(); } }

namespace Auth {

// static
TokenPair TokenFactory::GeneratePair(const UserInfo& user)
{
    const auto& uid      = user.getUid();
    const auto  jti_a    = Utils::Authentication::GenerateUid();
    const auto  jti_r    = Utils::Authentication::GenerateUid();

    TokenPair pair;
    pair.access.value = TokenService::GetInstance().Sign(
        uid, user.getAccount(), user.getUsername(), user.getAvatar(),
        TokenType::Access, AccessTokenTTL, jti_a);
    pair.access.ttl = AccessTokenTTL;

    pair.refresh.value = TokenService::GetInstance().Sign(
        uid, {}, {}, {},
        TokenType::Refresh, RefreshTokenTTL, jti_r);
    pair.refresh.jti = jti_r;
    pair.refresh.ttl = RefreshTokenTTL;

    return pair;
}

// static
AccessToken TokenFactory::GenerateAccess(const UserInfo& user)
{
    const auto jti = Utils::Authentication::GenerateUid();
    AccessToken at;
    at.value = TokenService::GetInstance().Sign(
        user.getUid(), user.getAccount(), user.getUsername(), user.getAvatar(),
        TokenType::Access, AccessTokenTTL, jti);
    at.ttl = AccessTokenTTL;
    return at;
}

// static
RefreshToken TokenFactory::GenerateRefresh(const std::string& uid)
{
    const auto jti = Utils::Authentication::GenerateUid();
    RefreshToken rt;
    rt.value = TokenService::GetInstance().Sign(
        uid, {}, {}, {},
        TokenType::Refresh, RefreshTokenTTL, jti);
    rt.jti = jti;
    rt.ttl = RefreshTokenTTL;
    return rt;
}

} // namespace Auth
