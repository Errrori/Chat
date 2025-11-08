#include "pch.h"
#include <regex>
#include <drogon/utils/Utilities.h>
#include <fstream>
#include <json/json.h>
#include "jwt-cpp/jwt.h"
#include <random>
#include "Common/User.h"


namespace Utils {
    namespace Authentication
    {
        bool IsValidAccount(const std::string& account) {
            //if (!DatabaseManager::ValidateAccount(account))
            //{
            //    return false;
            //}
            static const std::regex limit{ "^[a-zA-Z0-9]{6,16}$" };
            if (!std::regex_match(account, limit)) {
                return false;
            }
            static const std::vector<std::string> reservedWords = { "admin", "root", "system" };
            if (std::find(reservedWords.begin(), reservedWords.end(), account) != reservedWords.end())
            {
                return false;
            }
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
        std::string GenerateUid() {  // Removed Utils:: qualifier
            uuid_t uuid;
            uuid_generate(uuid);

            char str[37];
            uuid_unparse(uuid, str);

            return std::string(str);
        }
#endif

        std::string PasswordHashed(const std::string& password)
        {
            return drogon::utils::getMd5(password);
        }

        std::string GenerateSecret(size_t len)
        {
            constexpr auto charset = "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";

            std::string result;
            result.resize(len);

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<size_t> dis(0, strlen(charset) - 1);

            for (size_t i = 0; i < len; ++i)
            {
                result[i] = charset[dis(gen)];
            }

            return result;
        }

        std::string LoadJwtSecret(const std::string& file_path)
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

        std::string GetJwtSecret()
        {
            static std::string secret = LoadJwtSecret();
            if (secret.empty())
            {
                LOG_ERROR << "fail to get secret";
                return {};
            }

            return secret;
        }

    	//before use this function,make sure the user information is correct
        std::string GenerateJWT(const UserInfo& info)
        {
            auto secret = GetJwtSecret();
            if (secret.empty())
            {
                LOG_ERROR << "Error to load jwt secret";
                return {};
            }

            auto now = std::chrono::system_clock::now();
            auto expiry = now + std::chrono::seconds{ EffectiveTime };

            auto token = jwt::create()
                .set_type("JWT")
                .set_algorithm("HS256")
                .set_issued_at(now)
                .set_expires_at(expiry)
                .set_payload_claim("uid", jwt::claim(info.getUid()))
                .set_payload_claim("username", jwt::claim(info.getUsername()))
                .set_payload_claim("account", jwt::claim(info.getAccount()))
				.set_payload_claim("avatar",jwt::claim(info.getAvatar()))
                .sign(jwt::algorithm::hs256{ secret });

            return token;
        }

        bool VerifyJWT(const std::string& token, UsersInfo& info)
        {
            try
            {
                const auto& data = jwt::decode(token);

                auto verifier = jwt::verify()
                    .allow_algorithm(jwt::algorithm::hs256{ GetJwtSecret() }); // Modified: use GetJwtSecret()
                verifier.verify(data);

                auto exp = data.get_expires_at();
                if (exp < std::chrono::system_clock::now())
                {
                    LOG_ERROR << "token is not effective!"; // Modified: remove extra \n
                    return false;
                }

                info.username = data.get_payload_claim("username").as_string();
                info.uid = data.get_payload_claim("uid").as_string();
                info.avatar = data.get_payload_claim("avatar").as_string();
                info.account = data.get_payload_claim("account").as_string();
                return true;
            }
            catch (const std::exception& e)
            {
                LOG_ERROR << "Error: " << e.what();
                return false;
            }
        }

        std::string GetToken(const drogon::HttpRequestPtr& req)
        {
            std::string token{};
            //1.find token in authorization
            std::string authHeader = req->getHeader("Authorization");
            if (!authHeader.empty()) {
                if (authHeader.substr(0, 4) == "JWT ") {
                    token = authHeader.substr(4);
                }
                else {
                    token = authHeader;
                }
            }
            // 2.find token in Json
            else if (auto jsonObj = req->getJsonObject(); jsonObj && jsonObj->isMember("token")) {
                token = (*jsonObj)["token"].asString();
            }
            // 3.find token in parameter
            else if (!req->getParameter("token").empty()) {
                token = req->getParameter("token");
            }
            return token;
        }
    }

    // 统一错误响应处理函数实现
    drogon::HttpResponsePtr CreateErrorResponse(int statusCode, int code, const std::string& message)
    {
        Json::Value response;
        response["code"] = code;
        response["message"] = message;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));
        return resp;
    }

    drogon::HttpResponsePtr CreateSuccessResp(int statusCode, int code, const std::string& message)
    {
        Json::Value response;
        response["code"] = code;
        response["message"] = message;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));
        return resp;
    }

    drogon::HttpResponsePtr CreateSuccessJsonResp(int statusCode, int code, const std::string& message,const Json::Value& data)
    {
        Json::Value response;
        response["code"] = code;
        response["message"] = message;
        response["data"] = data;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));
        return resp;
    }

    Json::Value GenErrorResponse(const std::string& msg, ChatCode::Code code)
    {
        Json::Value resp;
        resp["code"] = code;
        resp["error"] = msg;
        return resp;
    }

    //only for Ai request
    Json::Value GenErrorResponse(const std::string& msg, ChatCode::Code code,const std::string& message_id)
    {
        Json::Value resp;
        resp["code"] = code;
        resp["error"] = msg;
        resp["message_id"] = message_id;
        return resp;
    }

    std::string GetCurrentTimeStr()
    {
        return trantor::Date::now().toDbString();
    }

    int64_t GetCurrentTimeStamp()
    {
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
}
