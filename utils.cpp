#include "pch.h"
#include <regex>
#include <drogon/utils/Utilities.h>
#include <fstream>
#include <json/json.h>
#include "jwt-cpp/jwt.h"
#include <random>
#include <cctype>
#include <filesystem>
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
            namespace fs = std::filesystem;

            auto loadSecretFromFile = [](const fs::path& path) -> std::string {
                std::ifstream file(path.string());
                if (!file.is_open())
                {
                    return {};
                }

                Json::Value root;
                file >> root;
                if (!root.isMember("jwt_secret"))
                {
                    LOG_ERROR << "can not find secret in file: " << path.string();
                    return {};
                }
                return root["jwt_secret"].asString();
            };

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
                    auto secret = loadSecretFromFile(candidate);
                    if (!secret.empty())
                    {
                        return secret;
                    }
                }
            }

            // No existing file found, create one using the provided relative path.
            const auto writeTarget = fs::path(file_path);
            auto secret = GenerateSecret();
            Json::Value data;
            data["jwt_secret"] = secret;

            std::ofstream ofile(writeTarget.string());
            if (!ofile.is_open())
            {
                LOG_ERROR << "fail to generate secret file: " << writeTarget.string();
                return {};
            }
            Json::StreamWriterBuilder builder;
            builder["indentation"] = "";
            ofile << Json::writeString(builder, data);
            ofile.close();
            return secret;
        }

        std::string GetJwtSecret()
        {
            static std::string secret = LoadJwtSecret(SecretFilePath);
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

        bool VerifyJWT(const std::string& token, UserInfo& info)
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

                info.setUsername(data.get_payload_claim("username").as_string());
                info.setUid(data.get_payload_claim("uid").as_string());
                info.setAvatar(data.get_payload_claim("avatar").as_string());
                info.setAccount(data.get_payload_claim("account").as_string());
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
            // 1. Find token in Authorization header.
            // Accept both "Bearer <token>" and "JWT <token>" for compatibility.
            std::string authHeader = req->getHeader("Authorization");
            if (!authHeader.empty()) {
                // Trim surrounding spaces.
                auto begin = authHeader.find_first_not_of(" \t\r\n");
                if (begin != std::string::npos)
                {
                    auto end = authHeader.find_last_not_of(" \t\r\n");
                    authHeader = authHeader.substr(begin, end - begin + 1);
                }

                auto startsWithNoCase = [](const std::string& s, const std::string& prefix) {
                    if (s.size() < prefix.size())
                    {
                        return false;
                    }
                    for (size_t i = 0; i < prefix.size(); ++i)
                    {
                        if (std::tolower(static_cast<unsigned char>(s[i])) !=
                            std::tolower(static_cast<unsigned char>(prefix[i])))
                        {
                            return false;
                        }
                    }
                    return true;
                };

                if (startsWithNoCase(authHeader, "Bearer "))
                {
                    token = authHeader.substr(7);
                }
                else if (startsWithNoCase(authHeader, "JWT "))
                {
                    token = authHeader.substr(4);
                }
                else
                {
                    // Backward compatibility: allow raw token value in header.
                    token = authHeader;
                }
            }
            // 2. Find token in JSON body.
            else if (auto jsonObj = req->getJsonObject(); jsonObj && jsonObj->isMember("token")) {
                token = (*jsonObj)["token"].asString();
            }
            // 3. Find token in query parameter.
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
}
