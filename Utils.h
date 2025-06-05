#pragma once
#include <string>

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

    namespace Type
    {
        #define CONTENT_TYPE_MEMBERS \
        	X(Text, "Text") \
        	X(Image, "Image") \
            X(File, "File") \
            X(Video, "Video") \
            X(Audio, "Audio") \
            X(ErrorMsg, "ErrorMsg") \
            X(Unknown, "Unknown")
        
        #define CHAT_TYPE_MEMBERS \
            X(Group, "Group") \
            X(Private, "Private") \
            X(System, "System") \
            X(Public, "Public") \
            X(Unknown, "Unknown")
        
        #define RELATIONSHIP_ACTION_TYPE \
       		X(FriendRequest,"FriendRequest")\
            X(RequestAccept,"RequestAccept")\
       		X(RequestReject,"RequestReject")\
       		X(BlockUser,"BlockUser")\
       		X(UnblockUser,"UnblockUser")\
       		X(Unfriend,"Unfriend")\
            X(Unknown, "Unknown")

		#define RELATIONSHIP_STATUS_MEMBERS \
			X(Pending,"Pending")\
			X(Friend,"Friend")\
			X(Blocking,"Blocking")\
			X(Unknown, "Unknown")

		#define GROUP_ACTION_TYPE \
            X(GroupRequest,"GroupRequest")\
			X(GroupAccept,"GroupAccept")
        //...
		//待定，还没写完


        #define NOTIFICATION_SOURCE \
            X(System,"System")\
            X(Relationship,"Relationship")\
            X(Group,"Group")\
        	X(Unknown, "Unknown")

    	}

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
    
    namespace Message
	{
        Json::Value GenerateErrorMsg(const std::string& error_msg);
        int64_t GenerateMsgId();

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

        namespace Content {
            /*enum class ContentType :std::uint8_t {
                Text,
                Image,
                File,
                Video,
                Audio,
                Relationship,
                ErrorMsg,
                Unknown
            };

            std::string TypeToString(ContentType type);
            ContentType StringToType(const std::string& type_str);
            bool IsRelationshipActionValid(const std::string& type_str);*/

            enum class ContentType : std::uint8_t {
				#define X(EnumName, StringName) EnumName,
            	CONTENT_TYPE_MEMBERS
				#undef X
			};

			inline std::string TypeToString(ContentType type) {
				switch (type)
				{
					#define X(EnumName, StringName) case ContentType::EnumName: return StringName;
						CONTENT_TYPE_MEMBERS
					#undef X
					default: return "Unknown";
				}
			}

            inline ContentType StringToType(const std::string& type_str)
        	{
				#define X(EnumName, StringName) if (type_str == StringName) return ContentType::EnumName;
					CONTENT_TYPE_MEMBERS
				#undef X
				return ContentType::Unknown;
			}

            inline bool IsValid(const std::string& type_str)
		    {
		    	#define X(EnumName, StringName) if (type_str == StringName) return true;
		    		CONTENT_TYPE_MEMBERS
		    	#undef X
                return false;
            }
        }

        namespace Chat
		{
            enum class ChatType : std::uint8_t {
				#define X(EnumName, StringName) EnumName,
            	CHAT_TYPE_MEMBERS
				#undef X
			};

			inline std::string TypeToString(ChatType type) {
				switch (type)
				{
					#define X(EnumName, StringName) case ChatType::EnumName: return StringName;
						CHAT_TYPE_MEMBERS
					#undef X
					default: return "Unknown";
				}
			}

            inline ChatType StringToType(const std::string& type_str)
        	{
				#define X(EnumName, StringName) if (type_str == StringName) return ChatType::EnumName;
					CHAT_TYPE_MEMBERS
				#undef X
				return ChatType::Unknown;
			}

            inline bool IsValid(const std::string& type_str)
		    {
		    	#define X(EnumName, StringName) if (type_str == StringName) return true;
					CHAT_TYPE_MEMBERS
		    	#undef X
                return false;
            }
        }
	}

    namespace Relationship
    {
	    enum class StatusType : std::uint8_t {
				#define X(EnumName, StringName) EnumName,
            		RELATIONSHIP_STATUS_MEMBERS
				#undef X
			};

        std::string TypeToString(StatusType type);

        StatusType StringToType(const std::string& type_str);

        bool IsValid(const std::string& type_str);

    }


    namespace UserAction
    {
        namespace RelationAction
        {
        	enum class RelationshipActionType : std::uint8_t {
				#define X(EnumName, StringName) EnumName,
            		RELATIONSHIP_ACTION_TYPE
				#undef X
			};

            std::string TypeToString(RelationshipActionType type);

            RelationshipActionType StringToType(const std::string& type_str);

            bool IsValid(const std::string& type_str);

            std::optional<Relationship::StatusType> ToStatus(RelationshipActionType action_type);
        }

        namespace GroupAction
    	{
        	enum class GroupActionType : std::uint8_t {
				#define X(EnumName, StringName) EnumName,
					GROUP_ACTION_TYPE
				#undef X
			};

			std::string TypeToString(GroupActionType type);

			GroupActionType StringToType(const std::string& type_str);

        	bool IsGroupActionValid(const std::string& type_str);
        }
    }

    namespace Notification
    {
	    enum class NotificationSource : std::uint8_t {
				#define X(EnumName, StringName) EnumName,
            		NOTIFICATION_SOURCE
				#undef X
			};

        std::string TypeToString(NotificationSource type);

        NotificationSource StringToType(const std::string& type_str);

        bool IsValid(const std::string& type_str);
    }

    // 统一错误响应处理函数
    drogon::HttpResponsePtr CreateErrorResponse(int statusCode, int code, const std::string& message);

};

