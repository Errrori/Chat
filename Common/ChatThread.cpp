#include "pch.h"
#include "ChatThread.h"

#include "Convert.h"
#include "Utils.h"
#include "models/GroupMembers.h"
#include "models/PrivateChats.h"
#include "models/AiChats.h"
#include "models/GroupChats.h"
#include "models/Threads.h"



std::unique_ptr<ChatThread> ChatThread::FromThreadId(int thread_id) {
    // 这里需要从数据库查询thread类型，然后调用对应的构造方法
    // 暂时返回nullptr，具体实现需要集成ThreadManager
    return nullptr;
}


ChatThread::ThreadType PrivateThread::GetThreadType() const {
    return ThreadType::PRIVATE;
}

bool PrivateThread::IsDbValid() const {
    return thread_id_ > 0 && !_first_uid.empty() && !_second_uid.empty()
		&& _first_uid < _second_uid;
}

bool PrivateThread::IsDataValid() const
{
    return !_first_uid.empty() && !_second_uid.empty()
		&& _first_uid < _second_uid;
}


Json::Value PrivateThread::ToJson() const {
    Json::Value json;
    json["thread_id"] = thread_id_;
    json["type"] = static_cast<int>(GetThreadType());
    json["uid1"] = _first_uid;
    json["uid2"] = _second_uid;
    return json;
}

drogon_model::postgres::Threads PrivateThread::ToDbThread() const {
    
    drogon_model::postgres::Threads thread;
    thread.setType(static_cast<int64_t>(GetThreadType()));
    thread.setCreateTime(Utils::GetCurrentTimeStamp());
    return thread;
}

std::string PrivateThread::GetDisplayName(const std::string& current_uid) const {
    if (current_uid.empty()) {
        return GetUserDisplayName(_first_uid) + " & " + GetUserDisplayName(_second_uid);
    }
    
    // 对于当前用户，显示对方的名字
    if (current_uid == _first_uid) {
        return GetUserDisplayName(_second_uid);
    } else if (current_uid == _second_uid) {
        return GetUserDisplayName(_first_uid);
    } else {
        // 当前用户不在此私聊中，显示组合名
        return GetUserDisplayName(_first_uid) + " & " + GetUserDisplayName(_second_uid);
    }
}

std::string PrivateThread::GetPeerUid(const std::string& current_uid) const {
    if (current_uid == _first_uid) {
        return _second_uid;
    }
    if (current_uid == _second_uid) {
        return _first_uid;
    }
    return "";  // 当前用户不在此私聊中
}

std::optional<drogon_model::postgres::PrivateChats> PrivateThread::ToDbPrivateChat() const {
    if (!IsDbValid()) {
        return std::nullopt;
    }
    
    drogon_model::postgres::PrivateChats privateChat;
    if (thread_id_>0)
		privateChat.setThreadId(thread_id_);
    privateChat.setUid1(_first_uid);
    privateChat.setUid2(_second_uid);
    return privateChat;
}

PrivateThread PrivateThread::FromJson(const Json::Value& json) {
    PrivateThread chat;
    chat.SetThreadId(json.get("thread_id", -1).asInt());
    chat.SetFirstUid(json.get("uid1", "").asString());
    chat.SetSecondUid(json.get("uid2", "").asString());
    return chat;
}

std::string PrivateThread::GetUserDisplayName(const std::string& uid) const {
    // 这里可以通过Container获取UserService来查询用户名
    // 或者使用现有的ConnectionManager::GetName方法
    return {};
}

// ===== GroupThread 实现 =====

ChatThread::ThreadType GroupThread::GetThreadType() const {
    return ThreadType::GROUP;
}

bool GroupThread::IsDbValid() const {
    return thread_id_ > 0 && !_name.empty()&&!_owner_uid.empty();
}

bool GroupThread::IsDataValid() const
{
    return !_name.empty()&&!_owner_uid.empty();
}

Json::Value GroupThread::ToJson() const {
    Json::Value json;
    json["thread_id"] = thread_id_;
    json["type"] = static_cast<int>(GetThreadType());
    json["name"] = _name;
    json["description"] = _description;
    json["avatar"] = _avatar;
    return json;
}

drogon_model::postgres::Threads GroupThread::ToDbThread() const {

    drogon_model::postgres::Threads thread;
    thread.setType(static_cast<int64_t>(GetThreadType()));
    thread.setCreateTime(Utils::GetCurrentTimeStamp());
    return thread;
}


std::string GroupThread::GetDisplayName(const std::string& current_uid) const {
    return _name;
}

std::string GroupThread::GetDescription() const {
    return _description;
}

std::string GroupThread::GetAvatar() const {
    return _avatar;
}

std::optional<drogon_model::postgres::GroupChats> GroupThread::ToDbGroupChat() const {
    if (!IsDbValid()) {
        return std::nullopt;
    }
    
    drogon_model::postgres::GroupChats groupChat;
    groupChat.setThreadId(thread_id_);
    groupChat.setName(_name);
    groupChat.setDescription(_description);
    groupChat.setAvatar(_avatar);
    return groupChat;
}

std::optional<drogon_model::postgres::GroupMembers> GroupThread::ToDbOwner() const
{
    if (_owner_uid.empty()||thread_id_<=0)
    {
        return std::nullopt;
    }
    drogon_model::postgres::GroupMembers members;
    members.setJoinTime(Utils::GetCurrentTimeStamp());
    members.setRole(GroupConstant::owner);
	members.setUserUid(_owner_uid);
    members.setThreadId(thread_id_);
    return members;
}

GroupThread GroupThread::FromJson(const Json::Value& json) {
    GroupThread chat;
    chat.SetThreadId(json.get("thread_id", -1).asInt());
    chat.SetName(json.get("name", "").asString());
    chat.SetDescription(json.get("description", "").asString());
    chat.SetAvatar(json.get("avatar", "").asString());
    chat.SetOwnerUid(json.get("uid", "").asString());
    return chat;
}

// ===== AIThread 实现 =====

ChatThread::ThreadType AIThread::GetThreadType() const {
    return ThreadType::AI;
}

bool AIThread::IsDbValid() const {
    return thread_id_ > 0 && !_name.empty() && !_creator_uid.empty();
}

bool AIThread::IsDataValid() const {
    return !_name.empty() && !_creator_uid.empty();
}

Json::Value AIThread::ToJson() const {
    Json::Value json;
    json["thread_id"] = thread_id_;
    json["type"] = static_cast<int>(GetThreadType());
    json["name"] = _name;
    json["creator_uid"] = _creator_uid;
    json["init_settings"] = _init_setting;
    json["avatar"] = _avatar;
    return json;
}

drogon_model::postgres::Threads AIThread::ToDbThread() const {
    drogon_model::postgres::Threads thread;
    thread.setType(static_cast<int64_t>(GetThreadType()));
    thread.setCreateTime(Utils::GetCurrentTimeStamp());
    return thread;
}

std::string AIThread::GetDisplayName(const std::string& current_uid) const {
    return _name;
}

std::string AIThread::GetAvatar() const {
    return _avatar;
}

std::optional<drogon_model::postgres::AiChats> AIThread::ToDbAiChat() const {
    if (!IsDbValid()) {
        return std::nullopt;
    }
    
    drogon_model::postgres::AiChats aiChat;
    if (thread_id_>0)
		aiChat.setThreadId(thread_id_);
    aiChat.setName(_name);
    aiChat.setCreatorUid(_creator_uid);
    aiChat.setInitSettings(_init_setting);
    aiChat.setAvatar(_avatar);
    return aiChat;
}

AIThread AIThread::FromJson(const Json::Value& json) {
    AIThread chat;
    chat.SetThreadId(json.get("thread_id", -1).asInt());
    chat.SetName(json.get("name", "").asString());
    chat.SetCreatorUid(json.get("creator_uid", "").asString());
    chat.SetInitSetting(json.get("init_settings", "").asString());
    chat.SetAvatar(json.get("avatar", "").asString());
    return chat;
}

// ===== MemberData 实现 =====

// 静态工厂方法实现
MemberData MemberData::FromJson(const Json::Value& json_data) {
    MemberData member;
    member.SetThreadId(json_data.get("thread_id", -1).asInt());
    member.SetUserUid(json_data.get("user_uid", "").asString());
    member.SetRole(json_data.get("role", GroupConstant::member).asInt());
    
    if (json_data.isMember("join_time")) {
        member.SetJoinTime(json_data.get("join_time", "").asInt64());
    } else {
        member.SetJoinTime(Utils::GetCurrentTimeStamp());
    }

    return member;
}

// JSON 序列化实现
Json::Value MemberData::ToJson() const {
    Json::Value json;
    json["thread_id"] = _thread_id;
    json["user_uid"] = _user_uid;
    json["role"] = GroupRoleConvert::ToVal(_role);
    json["join_time"] = (Json::Value::Int64)_join_time;
    return json;
}

// 数据库转换实现
std::optional<drogon_model::postgres::GroupMembers> MemberData::ToDbGroupMember() const {
    if (!IsValid()) {
        return std::nullopt;
    }

    drogon_model::postgres::GroupMembers member;
    member.setThreadId(_thread_id);
    member.setUserUid(_user_uid);
    member.setRole(GroupRoleConvert::ToVal(_role));
    member.setJoinTime(_join_time<0 ? Utils::GetCurrentTimeStamp() : _join_time);
    return member;
}

void MemberData::SetRole(int role)
{
    _role = GroupRoleConvert::ToRole(role);
}

// 验证方法实现
bool MemberData::IsValid() const {
    return _thread_id > 0 && !_user_uid.empty() && 
           (_role!=Role::Unknown);
}

