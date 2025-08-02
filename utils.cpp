#include "pch.h"
#include <regex>
#include <drogon/utils/Utilities.h>
#include <fstream>
#include <json/json.h>
#include "jwt-cpp/jwt.h"
#include <random>


namespace Utils {
    namespace Authentication
    {
        bool IsValidAccount(const std::string& account) {
            //if (!DatabaseManager::ValidateAccount(account))
            //{
            //    return false;
            //}
            static const std::regex limit{ "^[a-zA-Z0-9]{8,15}$" };
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

        std::string GenerateJWT(const UserInfo& info)
        {
            auto secret = GetJwtSecret();
            if (secret.empty())
            {
                LOG_ERROR << "Error to load jwt secret";
                return {};
            }

            auto name = info.username;
            auto uid = info.uid;
            auto account = info.account;
            auto avatar = info.avatar;

            auto now = std::chrono::system_clock::now();
            auto expiry = now + std::chrono::seconds{ EffectiveTime };

            auto token = jwt::create()
                .set_type("JWT")
                .set_algorithm("HS256")
                .set_issued_at(now)
                .set_expires_at(expiry)
                .set_payload_claim("uid", jwt::claim(uid))
                .set_payload_claim("username", jwt::claim(name))
                .set_payload_claim("account", jwt::claim(account))
				.set_payload_claim("avatar",jwt::claim(avatar))
                .sign(jwt::algorithm::hs256{ secret });

            return token;
        }

        bool VerifyJWT(const std::string& token, UserInfo& info)
        {
            try
            {
                const auto& data = jwt::decode(token);

                auto verifier = jwt::verify()
                    .allow_algorithm(jwt::algorithm::hs256{ GetJwtSecret() }); // <--- 修改：使用 GetJwtSecret()
                verifier.verify(data);

                auto exp = data.get_expires_at();
                if (exp < std::chrono::system_clock::now())
                {
                    LOG_ERROR << "token is not effective!"; // <--- 修改：移除多余的 \n
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

        Json::Value GenerateErrorMsg(const std::string& error_msg)
        {
            Json::Value error_json;
            error_json["content_type"] = "ErrorMsg";
            error_json["content"] = error_msg;
            return error_json;
        }
    }

    namespace Relationship
    {
		std::string TypeToString(StatusType type)
    {
    	switch (type)
    	{
			#define X(EnumName, StringName) case StatusType::EnumName: return StringName;
    			RELATIONSHIP_STATUS_MEMBERS
			#undef X
    		default: return "Unknown";
    	}
    }

    StatusType StringToType(const std::string& type_str)
    {
		#define X(EnumName, StringName) if (type_str == StringName) return StatusType::EnumName;
    		RELATIONSHIP_STATUS_MEMBERS
		#undef X
		return StatusType::Unknown;
	}

    bool IsValid(const std::string& type_str)
	{
	  	#define X(EnumName, StringName) if (type_str == StringName) return true;
          RELATIONSHIP_STATUS_MEMBERS
	  	#undef X
    	return false;
	}    
    }

    namespace UserAction
    {
        namespace RelationAction
        {
	         std::string TypeToString(RelationshipActionType type)
             {
                 switch (type)
                 {
	         		#define X(EnumName, StringName) case RelationshipActionType::EnumName: return StringName;
	         			RELATIONSHIP_ACTION_TYPE
	         		#undef X
             		default: return "Unknown";
                 }
             }

            RelationshipActionType StringToType(const std::string& type_str)
            {
                #define X(EnumName, StringName) if (type_str == StringName) return RelationshipActionType::EnumName;
	        		RELATIONSHIP_ACTION_TYPE
	        	#undef X
	        	return RelationshipActionType::Unknown;
            }

            bool IsValid(const std::string& type_str)
            {
                #define X(EnumName, StringName) if (type_str == StringName) return true;
	        		RELATIONSHIP_ACTION_TYPE
	          	#undef X
            	return false;
            }

            std::optional<Relationship::StatusType> ToStatus(RelationshipActionType action_type)
            {
	            switch (action_type)
	            {
	            case RelationshipActionType::Unfriend:
	            case RelationshipActionType::RequestReject:
	            case RelationshipActionType::UnblockUser:
                    return std::nullopt;
	            case RelationshipActionType::BlockUser:
                    return Relationship::StatusType::Blocking;
	            case RelationshipActionType::FriendRequest:
                    return Relationship::StatusType::Pending;
	            case RelationshipActionType::RequestAccept:
                    return Relationship::StatusType::Friend;
	            default:
                    return Relationship::StatusType::Unknown;
	            }
            }
        }
    }

    namespace Notification
    {
	        std::string TypeToString(NotificationSource type)
    {
        switch (type)
    	{
			#define X(EnumName, StringName) case NotificationSource::EnumName: return StringName;
				NOTIFICATION_SOURCE
			#undef X
    		default: return "Unknown";
    	}
    }

    NotificationSource StringToType(const std::string& type_str)
    {
        #define X(EnumName, StringName) if (type_str == StringName) return NotificationSource::EnumName;
			NOTIFICATION_SOURCE
		#undef X
		return NotificationSource::Unknown;
    }

    bool IsValid(const std::string& type_str)
    {
        #define X(EnumName, StringName) if (type_str == StringName) return true;
			NOTIFICATION_SOURCE
	  	#undef X
    	return false;
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
}
