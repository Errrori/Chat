#pragma once
#include <string>

class UserInfo;

namespace Auth {

struct AccessToken
{
    std::string value;
    unsigned    ttl;    // 有效期（秒）
};

struct RefreshToken
{
    std::string value;
    std::string jti;    // JWT ID，用于 Redis session 追踪
    unsigned    ttl;    // 有效期（秒）
};

/**
 * TokenPair — 登录时一次性签发 access + refresh。
 */
struct TokenPair
{
    AccessToken  access;
    RefreshToken refresh;
};

/**
 * TokenFactory — 高级工厂，调用方不需要关心 TTL、claims 细节。
 *
 * 使用场景：
 *   登录:    TokenFactory::GeneratePair(user)
 *   刷新:    TokenFactory::GenerateAccess(user) + TokenFactory::GenerateRefresh(uid)
 */
class TokenFactory
{
public:
    /// 登录时：同时生成 access + refresh token 对
    static TokenPair GeneratePair(const UserInfo& user);

    /// 刷新时：仅生成新 access token（保持与现有 UserInfo 同步）
    static AccessToken GenerateAccess(const UserInfo& user);

    /// 刷新时：生成新 refresh token（轮换，jti 全新）
    static RefreshToken GenerateRefresh(const std::string& uid);

private:
    TokenFactory() = delete;
};

} // namespace Auth
