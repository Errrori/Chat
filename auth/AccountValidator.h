#pragma once
#include <string>

namespace Auth {

/**
 * AccountValidator — 账号合法性校验。
 * 规则：6-16 位字母/数字，不能是保留词。
 */
class AccountValidator
{
public:
    static AccountValidator& GetInstance();

    /// 返回 true 表示账号格式合法且非保留词
    bool IsValid(const std::string& account) const;

private:
    AccountValidator() = default;
    AccountValidator(const AccountValidator&) = delete;
    AccountValidator& operator=(const AccountValidator&) = delete;
};

} // namespace Auth
