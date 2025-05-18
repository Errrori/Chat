#pragma once
#include <string>
#include <regex>

namespace Utils
{
    constexpr unsigned EffectiveTime = 30 * 24 * 60 * 60;
    constexpr unsigned SecretLength = 32;
    const std::string SecretFilePath = "jwt_secret.json";

	struct UserInfo
	{
		std::string username;
		std::string uid;
        std::string account;
	};

	namespace Authentication
	{
        bool IsValidAccount(const std::string& account);
        int64_t GenerateMsgId();
        std::string GenerateUid();
        std::string GenerateSecret(size_t len = SecretLength);
        std::string GetJwtSecret();
        std::string PasswordHashed(const std::string& password);
        std::string LoadJwtSecret(const std::string& file_path = SecretFilePath);
        std::string GenerateJWT(const UserInfo& info);
        bool VerifyJWT(const std::string& token, UserInfo& info);
	}
    
    namespace Message
	{
        class MessageIDGenerator
        {
        public:
            MessageIDGenerator(const MessageIDGenerator&) = delete;
            MessageIDGenerator& operator=(const MessageIDGenerator&) = delete;
            static MessageIDGenerator& GetInstance();
            int64_t NextId();
            // set worker id and data center id
            void SetMachineId(int workerId, int dataCenterId);

        private:
            MessageIDGenerator();
            ~MessageIDGenerator() = default;

            // start timestamp : (2023-01-01 00:00:00 UTC)
            static constexpr int64_t START_TIMESTAMP = 1672531200000;
            //the number of bits occupied by each part
            static constexpr int WORKER_ID_BITS = 5;
            static constexpr int DATACENTER_ID_BITS = 5;
            static constexpr int SEQUENCE_BITS = 12;
            //the max value of each part
            static constexpr int MAX_WORKER_ID = (1 << WORKER_ID_BITS) - 1;
            static constexpr int MAX_DATACENTER_ID = (1 << DATACENTER_ID_BITS) - 1;
            static constexpr int MAX_SEQUENCE = (1 << SEQUENCE_BITS) - 1;
            //left shift of each part
            static constexpr int WORKER_ID_SHIFT = SEQUENCE_BITS;
            static constexpr int DATACENTER_ID_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS;
            static constexpr int TIMESTAMP_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS + DATACENTER_ID_BITS;

            int workerId_;
            int datacenterId_;
            int64_t sequence_;
            int64_t lastTimestamp_;
            std::mutex mutex_;

            int64_t GetCurrentTimestamp();
            //wait for next millisecond
            int64_t WaitNextMillis(int64_t lastTimestamp);
        };

        int64_t GenerateMsgId();

        namespace MessageType {
            enum class Type :std::uint8_t {
                TEXT,
                IMAGE,
                FILE,
                VIDEO,
                AUDIO,
                SYSTEM,
                NOTICE,
                ERROR_MSG,
                UNKNOWN
            };

            std::string TypeToString(const Type& type);
            Type StringToType(const std::string& type_str);
            bool IsValid(const std::string& type_str);
        }
	}
    // 统一错误响应处理函数
    drogon::HttpResponsePtr CreateErrorResponse(int statusCode, int code, const std::string& message);
};

