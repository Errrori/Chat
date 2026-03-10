#include "pch.h"
#include "SecretProvider.h"
#include "TokenConstants.h"
#include <fstream>
#include <filesystem>
#include <json/json.h>
#include <random>
#include <cstring>

namespace Auth {

SecretProvider& SecretProvider::GetInstance()
{
    static SecretProvider instance;
    return instance;
}

const std::string& SecretProvider::GetJwtSecret()
{
    if (_cached_secret.empty())
        _cached_secret = LoadFromFile(SecretFilePath);
    return _cached_secret;
}

// static
std::string SecretProvider::GenerateRandomSecret(size_t len)
{
    constexpr auto charset =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    std::string result;
    result.resize(len);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, std::strlen(charset) - 1);

    for (size_t i = 0; i < len; ++i)
        result[i] = charset[dis(gen)];

    return result;
}

// static
std::string SecretProvider::LoadFromFile(const std::string& file_path)
{
    namespace fs = std::filesystem;

    auto tryRead = [](const fs::path& path) -> std::string {
        std::ifstream file(path.string());
        if (!file.is_open()) return {};
        Json::Value root;
        file >> root;
        if (!root.isMember("jwt_secret")) return {};
        return root["jwt_secret"].asString();
    };

    // 按优先级尝试多个候选路径
    const std::vector<fs::path> candidates = {
        fs::path(file_path),
        fs::path("ChatServer") / file_path,
        fs::path("..") / file_path,
        fs::path("..") / "ChatServer" / file_path
    };

    for (const auto& candidate : candidates)
    {
        if (fs::exists(candidate))
        {
            auto secret = tryRead(candidate);
            if (!secret.empty()) return secret;
        }
    }

    // 不存在则生成并写入
    auto secret = GenerateRandomSecret(SecretLength);
    Json::Value data;
    data["jwt_secret"] = secret;

    std::ofstream ofile(file_path);
    if (!ofile.is_open())
    {
        LOG_ERROR << "[SecretProvider] Failed to write secret file: " << file_path;
        return {};
    }
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    ofile << Json::writeString(builder, data);
    ofile.close();

    LOG_INFO << "[SecretProvider] Generated and stored new JWT secret.";
    return secret;
}

} // namespace Auth
