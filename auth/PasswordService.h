#pragma once
#include <string>

namespace Auth {

/**
 * PasswordService — 密码哈希与验证。
 * 当前使用 MD5（与现有数据库兼容），接口隔离后升级哈希算法只改此类。
 */
class PasswordService
{
public:
    static PasswordService& GetInstance();

    /// 对明文密码做哈希，返回存储用字符串
    std::string Hash(const std::string& plain) const;

    /// 校验明文与存储哈希是否匹配
    bool Verify(const std::string& plain, const std::string& stored_hash) const;

private:
    PasswordService() = default;
    PasswordService(const PasswordService&) = delete;
    PasswordService& operator=(const PasswordService&) = delete;
};

} // namespace Auth
