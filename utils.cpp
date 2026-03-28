#include "pch.h"
#include <regex>
#include <drogon/utils/Utilities.h>
#include <fstream>
#include <json/json.h>
#include <random>
#include <cctype>
#include <filesystem>
#include "Common/User.h"
#include "auth/SecretProvider.h"
#include "auth/TokenService.h"
#include "auth/TokenFactory.h"
#include "auth/PasswordService.h"


namespace Utils {
    namespace Authentication
    {
        bool IsValidAccount(const std::string& account) {
            static const std::regex fmt{ "^[a-zA-Z0-9]{6,16}$" };
            if (!std::regex_match(account, fmt))
                return false;

            //static const std::vector<std::string> reserved = { "admin", "root", "system" };
            //auto lower = account;
            //std::transform(lower.begin(), lower.end(), lower.begin(),
            //               [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

            //return std::find(reserved.begin(), reserved.end(), lower) == reserved.end();
            return true;
        }

#ifdef _WIN32
        // Windows implementation
#include <rpc.h>
        std::string GenerateUid() {
            UUID uuid;
            UuidCreate(&uuid);

            unsigned char* str;
            UuidToStringA(&uuid, &str);

            std::string result(reinterpret_cast<char*>(str));
            RpcStringFreeA(&str);

            return result;
        }

#else
        // Linux implementation
#include <uuid/uuid.h>
        std::string GenerateUid() {
            uuid_t uuid;
            uuid_generate(uuid);

            char str[37];
            uuid_unparse(uuid, str);

            return std::string(str);
        }
#endif

        std::string PasswordHashed(const std::string& password)
        {
            return Auth::PasswordService::GetInstance().Hash(password);
        }

        std::string GenerateSecret(size_t len)
        {
            return Auth::SecretProvider::GenerateRandomSecret(len);
        }

        std::string LoadJwtSecret(const std::string& /*file_path*/)
        {
        	return Auth::SecretProvider::GetInstance().GetJwtSecret();
        }

        std::string GetJwtSecret()
        {
            return Auth::SecretProvider::GetInstance().GetJwtSecret();
        }

        std::string GetToken(const drogon::HttpRequestPtr& req)
        {
            return Auth::TokenService::GetInstance().ExtractFromRequest(req);
        }
    }



    void SendJson(const drogon::WebSocketConnectionPtr& conn, const Json::Value& data)
    {
        if (!conn || !conn->connected()) return;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        auto payload = Json::writeString(builder, data);
        conn->send(payload, drogon::WebSocketMessageType::Text);
    }

    std::string GetCurrentTimeStr()
    {
        return trantor::Date::now().toDbString();
    }

    int64_t GetCurrentTimeStamp()
    {
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    double GetRandomJitter(double min_sec, double max_sec)
    {
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<double> dist(min_sec, max_sec);
        return dist(rng);
    }
}
