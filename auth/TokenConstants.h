#pragma once
#include <string>

namespace Auth {

    // ── Token 有效期（秒）──
    constexpr unsigned AccessTokenTTL  = 15 * 60;             // 15 分钟
    constexpr unsigned RefreshTokenTTL = 14 * 24 * 60 * 60;   // 14 天

    // Redis refresh session TTL（比 token 多 1 小时，保证 token 刚好过期时 session 仍可查到）
    constexpr int RefreshSessionTTL = static_cast<int>(RefreshTokenTTL) + 3600;

    // JWT typ claim 值
    constexpr auto AccessTokenType  = "access";
    constexpr auto RefreshTokenType = "refresh";

    // Redis key 格式：{} 占位符替换为 uid
    constexpr auto RefreshSessionKey = "auth:refresh:{}";  // 单端：每 uid 唯一，新登录覆盖旧 session

    // 密钥相关
    constexpr unsigned SecretLength = 32;
    inline const std::string SecretFilePath = "jwt_secret.json";

} // namespace Auth
