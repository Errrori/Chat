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
#include "auth/AccountValidator.h"


namespace Utils {
    namespace Authentication
    {
        bool IsValidAccount(const std::string& account) {
            return Auth::AccountValidator::GetInstance().IsValid(account);
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

    	std::string GenerateJWT(const UserInfo& info)
        {
                return Auth::TokenFactory::GenerateAccess(info.getUid()).value;
        }

            bool VerifyJWT(const std::string& token, std::string& uid)
        {
            std::string jti;
                return Auth::TokenService::GetInstance().Verify(token, Auth::TokenType::Access, uid, jti);
        }

        std::string GetToken(const drogon::HttpRequestPtr& req)
        {
            return Auth::TokenService::GetInstance().ExtractFromRequest(req);
        }
    }

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
