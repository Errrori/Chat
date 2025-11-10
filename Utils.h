#pragma once
#include <string>
class UserInfo;

namespace Utils
{
    constexpr unsigned EffectiveTime = 30 * 24 * 60 * 60;
    constexpr unsigned SecretLength = 32;
    const std::string SecretFilePath = "jwt_secret.json";

    drogon::HttpResponsePtr CreateErrorResponse(int statusCode, int code, const std::string& message);
    drogon::HttpResponsePtr CreateSuccessResp(int statusCode, int code, const std::string& message);
    drogon::HttpResponsePtr CreateSuccessJsonResp(int statusCode, int code, const std::string& message, const Json::Value& data);

    Json::Value GenErrorResponse(const std::string& msg, ChatCode::Code code);
    Json::Value GenErrorResponse(const std::string& msg, ChatCode::Code code, const std::string& message_id);

    std::string GetCurrentTimeStr();
    int64_t GetCurrentTimeStamp();

	namespace Authentication
	{
        bool IsValidAccount(const std::string& account);
        std::string GenerateUid();
        std::string GenerateSecret(size_t len = SecretLength);
        std::string GetJwtSecret();
        std::string PasswordHashed(const std::string& password);
        std::string LoadJwtSecret(const std::string& file_path = SecretFilePath);
        std::string GenerateJWT(const UserInfo& info);
        bool VerifyJWT(const std::string& token, UserInfo& info);
        std::string GetToken(const drogon::HttpRequestPtr& req);
	}
};

