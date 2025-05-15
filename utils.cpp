#include "pch.h"
#include <drogon/utils/Utilities.h>
#include <fstream>
#include <json/json.h>
#include "jwt-cpp/jwt.h"
#include <random>
#include <openssl/rand.h>


namespace Utils {
    namespace Authentication
    {
#ifdef _WIN32
        // Windows实现
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
        // Linux实现
#include <uuid/uuid.h>
        std::string GenerateUid() {  // 移除了Utils::限定符
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

        std::string GenJWT(const UserInfo& info)
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
            auto expiry = now + std::chrono::seconds{ EffectiveTime };

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

        bool VerifyJWT(const std::string& token, UserInfo& info)
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
                    LOG_ERROR << "token is not effective!\n\n";
                    return false;
                }

                info.username = data.get_payload_claim("username").as_string();
                info.uid = data.get_payload_claim("uid").as_string();
                return true;
            }
            catch (const std::exception& e)
            {
                LOG_ERROR << "Error: " << e.what();
                return false;
            }
        }
    }

    namespace Message
    {
        MessageIDGenerator& MessageIDGenerator::GetInstance() {
            static MessageIDGenerator instance;
            return instance;
        }

        MessageIDGenerator::MessageIDGenerator()
            : workerId_(0), datacenterId_(0), sequence_(0), lastTimestamp_(0) {
        }

        void MessageIDGenerator::SetMachineId(int workerId, int dataCenterId) {
            if (workerId > MAX_WORKER_ID || workerId < 0) {
                LOG_ERROR << "workerId can't be greater than " << MAX_WORKER_ID << " or less than 0";
                return;
            }
            if (dataCenterId > MAX_DATACENTER_ID || dataCenterId < 0) {
                LOG_ERROR << "datacenter Id can't be greater than " << MAX_DATACENTER_ID << " or less than 0";
                return;
            }
            workerId_ = workerId;
            datacenterId_ = dataCenterId;
        }

        int64_t MessageIDGenerator::NextId() {
            std::lock_guard<std::mutex> lock(mutex_);

            auto timestamp = GetCurrentTimestamp();

            // Clock moved backwards, refuse to generate id
            if (timestamp < lastTimestamp_) {
                LOG_ERROR << "Clock moved backwards. Refusing to generate id for " << (lastTimestamp_ - timestamp) << " milliseconds";
                return -1;
            }

            // same ms generate
            if (lastTimestamp_ == timestamp) {
                sequence_ = (sequence_ + 1) & MAX_SEQUENCE;
                // overflow ms
                if (sequence_ == 0) {
                    timestamp = WaitNextMillis(lastTimestamp_);
                }
            }
            else {
                sequence_ = 0;
            }

            lastTimestamp_ = timestamp;

            // build id
            return ((timestamp - START_TIMESTAMP) << TIMESTAMP_SHIFT)
                | (datacenterId_ << DATACENTER_ID_SHIFT)
                | (workerId_ << WORKER_ID_SHIFT)
                | sequence_;
        }

        int64_t MessageIDGenerator::GetCurrentTimestamp() {
            auto currentTimePoint = std::chrono::system_clock::now();
            auto current = std::chrono::duration_cast<std::chrono::milliseconds>(currentTimePoint.time_since_epoch()).count();
            return current;
        }

        int64_t MessageIDGenerator::WaitNextMillis(int64_t lastTimestamp) {
            auto timestamp = GetCurrentTimestamp();
            while (timestamp <= lastTimestamp) {
                timestamp = GetCurrentTimestamp();
            }
            return timestamp;
        }

        int64_t GenerateMsgId()
        {
            return MessageIDGenerator::GetInstance().NextId();
        }

        std::string MessageType::TypeToString(const Type& type)
        {
            switch (type) {
            case Type::TEXT: return "Text";
            case Type::IMAGE: return "Image";
            case Type::FILE: return "File";
            case Type::VIDEO: return "Video";
            case Type::AUDIO: return "Audio";
            case Type::SYSTEM: return "System";
            case Type::NOTICE: return "Notice";
            case Type::ERROR_MSG: return "ErrorMsg";
            default: return "Unknown";
            }
        }

        MessageType::Type MessageType::StringToType(const std::string& type_str)
        {
            static const std::unordered_map<std::string, Type> typeMap = {
           {"Text", Type::TEXT},
           {"Image", Type::IMAGE},
           {"File", Type::FILE},
           {"Video", Type::VIDEO},
           {"Audio", Type::AUDIO},
           {"System", Type::SYSTEM},
           {"Notice", Type::NOTICE},
           {"ErrorMsg", Type::ERROR_MSG}
            };
            auto it = typeMap.find(type_str);
            return (it != typeMap.end()) ? it->second : Type::UNKNOWN;
        }

        bool MessageType::IsValid(const std::string& type_str)
        {
            return StringToType(type_str) != Type::UNKNOWN;
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
        resp->addHeader("content-type", "application/json");
        return resp;
    }
}