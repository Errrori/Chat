#include "pch.h"
#include "PasswordService.h"
#include <drogon/utils/Utilities.h>

namespace Auth {

PasswordService& PasswordService::GetInstance()
{
    static PasswordService instance;
    return instance;
}

std::string PasswordService::Hash(const std::string& plain) const
{
    // 当前使用 MD5 与历史数据库保持兼容。
    // 若需升级至 bcrypt/Argon2，只改此函数即可，调用方无需修改。
    return drogon::utils::getMd5(plain);
}

bool PasswordService::Verify(const std::string& plain, const std::string& stored_hash) const
{
    return Hash(plain) == stored_hash;
}

} // namespace Auth
