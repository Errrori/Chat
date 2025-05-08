#include "Utils.h"
#include <drogon/utils/Utilities.h>

#ifdef _WIN32
// Windows实现
#include <rpc.h>
std::string Utils::GenerateUid() {
    UUID uuid;
    UuidCreate(&uuid);

    unsigned char* str;
    UuidToStringA(&uuid, &str);

    std::string result(reinterpret_cast<char*>(str));
    RpcStringFreeA(&str);

    return result;
}
#else
// Linux实现
#include <uuid/uuid.h>
std::string Utils::GenerateUid() {
    uuid_t uuid;
    uuid_generate(uuid);
    
    char str[37];
    uuid_unparse(uuid, str);
    
    return std::string(str);
}
#endif

std::string Utils::PasswordHashed(const std::string& password)
{
    return drogon::utils::getSha256(password);
}
