#include "pch.h"
#include "AccountValidator.h"
#include <regex>
#include <algorithm>

namespace Auth {

AccountValidator& AccountValidator::GetInstance()
{
    static AccountValidator instance;
    return instance;
}

bool AccountValidator::IsValid(const std::string& account) const
{
    static const std::regex fmt{ "^[a-zA-Z0-9]{6,16}$" };
    if (!std::regex_match(account, fmt))
        return false;

    static const std::vector<std::string> reserved = { "admin", "root", "system" };
    auto lower = account;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    return std::find(reserved.begin(), reserved.end(), lower) == reserved.end();
}

} // namespace Auth
