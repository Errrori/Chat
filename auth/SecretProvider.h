#pragma once
#include <string>
#include <mutex>

namespace Auth {

/**
 * SecretProvider — 负责从 jwt_secret.json 加载 JWT 签名密钥并缓存。
 * 单例，首次调用 GetJwtSecret() 时从文件加载，之后直接返回缓存。
 * 若文件不存在，自动生成并写入。
 */
class SecretProvider
{
public:
    static SecretProvider& GetInstance();

    /// 返回 JWT 签名密钥（首次调用时从文件加载）
    const std::string& GetJwtSecret();

    /// 生成指定长度的随机字符串（字母 + 数字）
    static std::string GenerateRandomSecret(size_t len = 32);

private:
    SecretProvider() = default;
    SecretProvider(const SecretProvider&) = delete;
    SecretProvider& operator=(const SecretProvider&) = delete;

    std::string _cached_secret;
    std::once_flag _load_once;

    static std::string LoadFromFile(const std::string& file_path);
};

} // namespace Auth
