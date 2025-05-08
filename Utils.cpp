#include "Utils.h"
#include <drogon/utils/Utilities.h>
#include <fstream>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include "jwt-cpp/jwt.h"
#include <chrono>
#include <openssl/rand.h>

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

std::string Utils::GenerateSecret(size_t len)
{
    std::string secret;
    std::vector<unsigned char> buffer(len);
    RAND_bytes(buffer.data(), static_cast<int>(len));

    static const char hex[] = "0123456789ABCDEF";
    for (unsigned char c : buffer) {
        secret.push_back(hex[c >> 4]);
        secret.push_back(hex[c & 15]);
    }

    return secret;
}

std::string Utils::LoadJwtSecret(const std::string& file_path)
{
    std::string secret;

    std::ifstream file("jwt_secret.json");
    if (!file.is_open())
    {
        std::ofstream ofile("jwt_secret.json");
        if (!ofile.is_open())
        {
            LOG_ERROR << "fail to generateSecret";
            return {};
        }
        secret = GenerateSecret();

        Json::Value data;
        data["jwt_secret"] = secret;
        ofile << data;
        ofile.close();
    }
    else
    {
        Json::Value root;
        file >> root;

        if (root.isMember("jwt_secret"))
        {
            secret = root["jwt_secret"].asString();
        }
        else
        {
            LOG_ERROR << "can not find secret";
            return {};
        }
    }
    return secret;
}

std::string Utils::GetJwtSecret()
{
    static std::string secret = LoadJwtSecret();
    if (secret.empty())
    {
        LOG_ERROR << "fail to get secret";
        return {};
    }

    return secret;
}

std::string Utils::GenJWT(UserInfo info)
{
    auto secret = GetJwtSecret();
    if (secret.empty())
    {
        LOG_ERROR << "Error to load jwt secret";
        return {};
    }

    auto name = info.username;
    auto uid = info.uid;

    auto now = std::chrono::system_clock::now();
    auto expiry = now + std::chrono::seconds{EffectiveTime};

    auto token = jwt::create()
        .set_type("JWT")
        .set_algorithm("HS256")
        .set_issued_at(now)
        .set_expires_at(expiry)
        .set_payload_claim("uid", jwt::claim(uid))
        .set_payload_claim("username", jwt::claim(name))
        .sign(jwt::algorithm::hs256{ secret });
    
    return token;
}

bool Utils::VerifyJWT(const std::string& token, UserInfo& info)
{
    try
    {
        auto data = jwt::decode(token);

        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{ LoadJwtSecret() });

        verifier.verify(data);

        auto exp = data.get_expires_at();
        if (exp < std::chrono::system_clock::now())
        {
            std::cout << "token is not effective!\n\n";
            return false;
        }

        info.username = data.get_payload_claim("username").as_string();
        info.uid = data.get_payload_claim("uid").as_string();
        return true;
    }
    catch (const std::exception& e)
    {
        std::cout << "Error: " << e.what() << "\n\n";
        LOG_ERROR << "Error: " << e.what();
        return false;
    }
}

