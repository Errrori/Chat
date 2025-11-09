#pragma once
#include <string>
#include <memory>
#include <optional>
#include <json/json.h>

namespace drogon_model::sqlite3
{
	class GroupMembers;
	class PrivateChats;
	class AiChats;
    class GroupChats;
    class Threads;
}

namespace GroupConstant
{
    constexpr int member = 0;
    constexpr int admin = 1;
    constexpr int owner = 2;

}

constexpr int TYPE_GROUP_CHAT = 0;
constexpr int TYPE_PRIVATE_CHAT = 1;
constexpr int TYPE_AI_CHAT = 2;

class ChatThread
{
public:
    enum ThreadType :std::int8_t
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
    virtual drogon_model::sqlite3::Threads ToDbThread() const = 0;
    
    virtual std::string GetDisplayName(const std::string& current_uid = "") const = 0;
    virtual std::string GetAvatar() const { return ""; }
    virtual std::string GetDescription() const { return ""; }
    
    void SetThreadId(int thread_id) { thread_id_ = thread_id; }
    int GetThreadId() const { return thread_id_; }

    static std::unique_ptr<ChatThread> FromThreadId(int thread_id);

protected:
    int thread_id_ = -1;
};


class PrivateThread : public ChatThread
{
public:
    ThreadType GetThreadType() const override;
    bool IsDbValid() const override;
    bool IsDataValid() const override;
    Json::Value ToJson() const override;
    drogon_model::sqlite3::Threads ToDbThread() const override;
    
    std::string GetDisplayName(const std::string& current_uid = "") const override;
    
    const std::string& GetFirstUid() const { return _first_uid; }
    const std::string& GetSecondUid() const { return _second_uid; }
    void SetFirstUid(const std::string& uid) { _first_uid = uid; }
    void SetSecondUid(const std::string& uid) { _second_uid = uid; }
    void SetUsers(const std::string& uid1, const std::string& uid2) {
        std::string first_uid, second_uid;
        if (uid1 < uid2) {
            first_uid = uid1;
            second_uid = uid2;
        } else {
            first_uid = uid2;
            second_uid = uid1;
		}
        _first_uid = first_uid;
        _second_uid = second_uid;
    }
    
    std::string GetPeerUid(const std::string& current_uid) const;
    
    std::optional<drogon_model::sqlite3::PrivateChats> ToDbPrivateChat() const;

    static PrivateThread FromJson(const Json::Value& json);

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
    drogon_model::sqlite3::Threads ToDbThread() const override;

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
    
    static GroupThread FromJson(const Json::Value& json);

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
    drogon_model::sqlite3::Threads ToDbThread() const override;
    
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
    
    static AIThread FromJson(const Json::Value& json);

private:
    std::string _name;
    std::string _creator_uid;
    std::string _avatar;
    std::string _init_setting;
};

class MemberData
{
public:
	enum class Role :std::int8_t
    {
		member = GroupConstant::member,
		admin = GroupConstant::admin,
		owner = GroupConstant::owner,
        Unknown = -1
    };

    static MemberData FromJson(const Json::Value& json_data);
    // JSON 序列化
    Json::Value ToJson() const;

    // 数据库转换
    std::optional<drogon_model::sqlite3::GroupMembers> ToDbGroupMember() const;

    // Setter 方法
	void SetThreadId(int thread_id) { _thread_id = thread_id; }
	void SetUserUid(const std::string& uid) { _user_uid = uid; }
    void SetRole(Role role) { _role = role; }
	void SetJoinTime(int64_t join_time) { _join_time = join_time; }
    void SetRole(int role);

    // Getter 方法
    int GetThreadId() const { return _thread_id; }
    const std::string& GetUserUid() const { return _user_uid; }
    Role GetRole() const { return _role; }
    int64_t GetJoinTime() const { return _join_time; }

    // 验证方法
    bool IsValid() const;

private:
	int _thread_id = -1;
	std::string _user_uid;
	Role _role = Role::Unknown;
    int64_t _join_time;
};

