#pragma once
#include <string>
#include <memory>
#include <optional>
#include <json/json.h>

#include "manager/ThreadManager.h"
#include "models/Threads.h"
#include "models/PrivateChats.h"
#include "models/GroupChats.h"
#include "models/AiChats.h"

namespace drogon_model::sqlite3
{
	class GroupMembers;
}

namespace GroupConstant
{
	namespace Role
	{
        constexpr int member = 0;
        constexpr int admin = 1;
        constexpr int owner = 2;
	}	
}

constexpr int TYPE_GROUP_CHAT = 0;
constexpr int TYPE_PRIVATE_CHAT = 1;
constexpr int TYPE_AI_CHAT = 2;

class ChatThread
{
public:
    enum class ThreadType :std::int8_t
    {
        GROUP = TYPE_GROUP_CHAT,
        PRIVATE = TYPE_PRIVATE_CHAT,
        AI = TYPE_AI_CHAT,
        UNKNOWN = -1
    };
    virtual ~ChatThread() = default;

    virtual ThreadType GetThreadType() const = 0;
    virtual bool IsDbValid() const = 0;
    virtual bool IsDataValid() const = 0;
    virtual Json::Value ToJson() const = 0;
    virtual std::optional<drogon_model::sqlite3::Threads> ToDbThread() const = 0;
    
    virtual std::string GetDisplayName(const std::string& current_uid = "") const = 0;
    virtual std::string GetAvatar() const { return ""; }
    virtual std::string GetDescription() const { return ""; }
    
    void SetThreadId(int thread_id) { thread_id_ = thread_id; }
    int GetThreadId() const { return thread_id_; }
    
    static std::shared_ptr<ChatThread> FromJson(const Json::Value& json);
    static std::unique_ptr<ChatThread> FromThreadId(int thread_id);

protected:
    int thread_id_ = -1;
};

namespace ThreadTypeConvert
{
    const std::unordered_map<int, ChatThreads::ThreadType> val_to_thread_map = {
		{TYPE_GROUP_CHAT, ChatThreads::ThreadType::GROUP},
		{TYPE_AI_CHAT, ChatThreads::ThreadType::AI},
		{TYPE_PRIVATE_CHAT, ChatThreads::ThreadType::PRIVATE}
    };

    // 从枚举到整数的映射
    const std::unordered_map<ChatThreads::ThreadType, int> thread_to_val_map = {
        {ChatThreads::ThreadType::GROUP, TYPE_GROUP_CHAT},
        {ChatThreads::ThreadType::AI, TYPE_AI_CHAT},
        {ChatThreads::ThreadType::PRIVATE, TYPE_PRIVATE_CHAT}
    };

    inline int ToVal(ChatThreads::ThreadType type)
    {
        if (type != ChatThreads::ThreadType::UNKNOWN)
            return thread_to_val_map.at(type);
    	return -1;
    }

    inline ChatThreads::ThreadType ToType(int val)
    {
	    auto it = val_to_thread_map.find(val);
        if (it != val_to_thread_map.end())
            return it->second;
        
    	return ChatThreads::ThreadType::UNKNOWN;
    }
}

class PrivateThread : public ChatThread
{
public:
    ThreadType GetThreadType() const override;
    bool IsDbValid() const override;
    bool IsDataValid() const override;
    Json::Value ToJson() const override;
    std::optional<drogon_model::sqlite3::Threads> ToDbThread() const override;
    
    std::string GetDisplayName(const std::string& current_uid = "") const override;
    
    const std::string& GetFirstUid() const { return _first_uid; }
    const std::string& GetSecondUid() const { return _second_uid; }
    void SetFirstUid(const std::string& uid) { _first_uid = uid; }
    void SetSecondUid(const std::string& uid) { _second_uid = uid; }
    void SetUsers(const std::string& uid1, const std::string& uid2) {
        _first_uid = uid1;
        _second_uid = uid2;
    }
    
    std::string GetPeerUid(const std::string& current_uid) const;
    
    std::optional<drogon_model::sqlite3::PrivateChats> ToDbPrivateChat() const;

    static std::shared_ptr<PrivateThread> FromJson(const Json::Value& json);

private:
    std::string _first_uid;
    std::string _second_uid;
    
    std::string GetUserDisplayName(const std::string& uid) const;
};

class GroupThread : public ChatThread
{
public:
    ThreadType GetThreadType() const override;
    bool IsDbValid() const override;
    bool IsDataValid() const override; 
    Json::Value ToJson() const override;
    std::optional<drogon_model::sqlite3::Threads> ToDbThread() const override;

    std::string GetDisplayName(const std::string& current_uid = "") const override;
    std::string GetDescription() const override;
    std::string GetAvatar() const override;
    
    std::string GetOwnerUId() const { return _owner_uid; }
    const std::string& GetName() const { return _name; }
    void SetName(const std::string& name) { _name = name; }
    void SetDescription(const std::string& desc) { _description = desc; }
    void SetAvatar(const std::string& avatar) { _avatar = avatar; }
    void SetOwnerUid(const std::string& uid) { _owner_uid = uid; }
    
    std::optional<drogon_model::sqlite3::GroupChats> ToDbGroupChat() const;
    std::optional<drogon_model::sqlite3::GroupMembers> ToDbOwner() const;
    
    static std::shared_ptr<GroupThread> FromJson(const Json::Value& json);

private:
    std::string _description;
    std::string _avatar;
    std::string _name;
    std::string _owner_uid;
};

class AIThread : public ChatThread
{
public:
    ThreadType GetThreadType() const override;
    bool IsDbValid() const override;
    bool IsDataValid() const override;  // 添加这一行
    Json::Value ToJson() const override;
    std::optional<drogon_model::sqlite3::Threads> ToDbThread() const override;
    
    std::string GetDisplayName(const std::string& current_uid = "") const override;
    std::string GetAvatar() const override;
    
    const std::string& GetName() const { return _name; }
    const std::string& GetCreatorUid() const { return _creator_uid; }
    const std::string& GetInitSetting() const { return _init_setting; }
    void SetName(const std::string& name) { _name = name; }
    void SetCreatorUid(const std::string& uid) { _creator_uid = uid; }
    void SetAvatar(const std::string& avatar) { _avatar = avatar; }
    void SetInitSetting(const std::string& setting) { _init_setting = setting; }
    
    std::optional<drogon_model::sqlite3::AiChats> ToDbAiChat() const;
    
    static std::shared_ptr<AIThread> FromJson(const Json::Value& json);

private:
    std::string _name;
    std::string _creator_uid;
    std::string _avatar;
    std::string _init_setting;
};


