#include "Utils.h"
#include <drogon/utils/Utilities.h>
#include <fstream>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include "jwt-cpp/jwt.h"
#include <chrono>
#include <openssl/rand.h>


namespace Utils {

    MessageIDGenerator& MessageIDGenerator::GetInstance() {
        static MessageIDGenerator instance;
        return instance;
    }

    MessageIDGenerator::MessageIDGenerator()
        : workerId_(0), datacenterId_(0), sequence_(0), lastTimestamp_(-1) {
    }

    void MessageIDGenerator::SetMachineId(int workerId, int dataCenterId) {
        if (workerId > MAX_WORKER_ID || workerId < 0) {
            throw std::invalid_argument("Worker ID can't be greater than " + std::to_string(MAX_WORKER_ID) + " or less than 0");
        }
        if (dataCenterId > MAX_DATACENTER_ID || dataCenterId < 0) {
            throw std::invalid_argument("Datacenter ID can't be greater than " + std::to_string(MAX_DATACENTER_ID) + " or less than 0");
        }
        workerId_ = workerId;
        datacenterId_ = dataCenterId;
    }

    int64_t MessageIDGenerator::NextId() {
        std::lock_guard<std::mutex> lock(mutex_);

        int64_t timestamp = GetCurrentTimestamp();

        //if current time is less than last id generated timestamp,
        //it indicates that the system clock has rolled back,throw exception
        if (timestamp < lastTimestamp_) {
            throw std::runtime_error("Clock moved backwards. Refusing to generate ID for " +
                std::to_string(lastTimestamp_ - timestamp) + " milliseconds");
        }

        //if generate in the same time,sorting in the same millisecond
        if (lastTimestamp_ == timestamp) {
            sequence_ = (sequence_ + 1) & MAX_SEQUENCE;
            // overflow
            if (sequence_ == 0) {
                //wait for next millisecond，get new timestamp
                timestamp = WaitNextMillis(lastTimestamp_);
            }
        }
        else {
            //timestamp changed,reset the sorting of the same millisecond
            sequence_ = 0;
        }
        lastTimestamp_ = timestamp;

        // Shift and combine them together through the OR operation to form a 64 - bit ID
        return ((timestamp - START_TIMESTAMP) << TIMESTAMP_SHIFT)
            | (datacenterId_ << DATACENTER_ID_SHIFT)
            | (workerId_ << WORKER_ID_SHIFT)
            | sequence_;
    }

    int64_t MessageIDGenerator::GetCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    int64_t MessageIDGenerator::WaitNextMillis(int64_t lastTimestamp) {
        int64_t timestamp = GetCurrentTimestamp();
        while (timestamp <= lastTimestamp) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            timestamp = GetCurrentTimestamp();
        }
        return timestamp;
    }

    int64_t GenerateMsgId()
    {
        return MessageIDGenerator::GetInstance().NextId();
    }

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
    std::string Utils::GenerateUid() {
        uuid_t uuid;
        uuid_generate(uuid);

        char str[37];
        uuid_unparse(uuid, str);

        return std::string(str);
    }
#endif

    std::string PasswordHashed(const std::string& password)
    {
        return drogon::utils::getSha256(password);
    }

    std::string GenerateSecret(size_t len)
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

}