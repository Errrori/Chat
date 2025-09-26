#pragma once
#include <string>
class UserInfo;

namespace Utils
{
    constexpr unsigned EffectiveTime = 30 * 24 * 60 * 60;
    constexpr unsigned SecretLength = 32;
    const std::string SecretFilePath = "jwt_secret.json";

	struct UsersInfo
	{
		std::string username;
		std::string uid;
        std::string account;
        std::string avatar;
        std::string email;
        std::string posts;
		std::string followers;
		std::string following;
		std::string create_time;
        std::string update_time;
		std::string last_login_time;
		std::string signature;
		int status = 0; // 0: normal, 1: banned

        std::string ToString() const
        {
            return "name: " + username + ", uid: " + uid + ", account: " + account + ", avatar: " + avatar+", email: " + email +
                   ", signature: " + signature + ", status: " + std::to_string(status) + ", posts: " + posts +
                   ", followers: " + followers + ", following: " + following + ", create_time: " + create_time +
				", update_time: " + update_time + ", last_login_time: " + last_login_time;
        }

        static UsersInfo FromJson(const Json::Value& json)
        {
            UsersInfo info{};
            if (json.isMember("username"))
				info.username = json["username"].asString();
        	if (json.isMember("avatar"))
        		info.avatar = json["avatar"].asString();
            if (json.isMember("email"))
        		info.email = json["email"].asString();
            if (json.isMember("signature"))
        		info.signature = json["signature"].asString();
            return info;
		}
	};

    drogon::HttpResponsePtr CreateErrorResponse(int statusCode, int code, const std::string& message);
    drogon::HttpResponsePtr CreateSuccessResp(int statusCode, int code, const std::string& message);
    drogon::HttpResponsePtr CreateSuccessJsonResp(int statusCode, int code, const std::string& message, const Json::Value& data);

    std::string GetCurrentTimeStr();

	namespace Authentication
	{
        bool IsValidAccount(const std::string& account);
        std::string GenerateUid();
        std::string GenerateSecret(size_t len = SecretLength);
        std::string GetJwtSecret();
        std::string PasswordHashed(const std::string& password);
        std::string LoadJwtSecret(const std::string& file_path = SecretFilePath);
        std::string GenerateJWT(const UserInfo& info);
        bool VerifyJWT(const std::string& token, UsersInfo& info);
        std::string GetToken(const drogon::HttpRequestPtr& req);
	}
};

