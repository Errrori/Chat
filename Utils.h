#pragma once
#include <string>
#include "auth/TokenConstants.h"

class UserInfo;

namespace Utils
{
    // Backward compatible aliases (new code should use Auth::AccessTokenTTL / Auth::RefreshTokenTTL directly)
    constexpr unsigned EffectiveTime = Auth::AccessTokenTTL;
    constexpr unsigned SecretLength  = Auth::SecretLength;
    inline const std::string& SecretFilePath = Auth::SecretFilePath;



    void SendJson(const drogon::WebSocketConnectionPtr& conn, const Json::Value& data);

    std::string GetCurrentTimeStr();
    int64_t GetCurrentTimeStamp();
    double GetRandomJitter(double min_sec = 0.0, double max_sec = 10.0);

    namespace Authentication
    {
        // UUID / Secret generation utilities
        std::string GenerateUid();
        std::string GenerateSecret(size_t len = Auth::SecretLength);

        // Password hashing (delegates to Auth::PasswordService)
        std::string PasswordHashed(const std::string& password);

        // Account validation (delegates to Auth::AccountValidator)
        bool IsValidAccount(const std::string& account);

        // Secret management (delegates to Auth::SecretProvider)
        std::string GetJwtSecret();
        std::string LoadJwtSecret(const std::string& file_path = Auth::SecretFilePath);

        // Extract token string from request (delegates to Auth::TokenService)
        std::string GetToken(const drogon::HttpRequestPtr& req);
    }
};

