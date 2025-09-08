#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H
#include <string>
#include "models/GroupMembers.h"
#include "models/GroupChats.h"

static constexpr int GROUP_TYPE = 0;
static constexpr int PRIVATE_TYPE = 1;
static constexpr int DEFAULT_ROLE = 0;
static constexpr int ADMIN_ROLE = 1;
static constexpr int MASTER_ROLE = 2;

struct GroupMemberData
{
    int thread_id{-1};
    std::string user_uid; 
    std::optional<int> role; // 0=member, 1=admin, 2=owner
    std::optional<std::vector<std::string>> members_uid;

    std::optional<drogon_model::sqlite3::GroupMembers> TransToGroupMembers() const;

    static std::optional<GroupMemberData> BuildFromJson(const Json::Value& data);
};

struct GroupChatData
{
    std::string name;
    std::optional<std::string> avatar;
    std::string description;

    std::optional<drogon_model::sqlite3::GroupChats> TransToGroupChats() const;
    static std::optional<GroupChatData> BuildFromJson(const Json::Value& data);
};

class ThreadManager
{
public:
    static std::optional<std::vector<std::string>> GetThreadMembersUid(int thread_id);
    static std::optional<Json::Value> GetGroupInfo(int thread_id);
    static Json::Value GetThreadInfo();
    static Json::Value GetPrivateChatInfo();
    static Json::Value GetUserChatList(const std::string& uid);
    static Json::Value GetUserThreadIds(const std::string& uid);

    static bool AddNewGroupMember(int thread_id, const std::string& user_id,int role = DEFAULT_ROLE);

    static std::optional<int> CreatePrivateThread(const std::string& uid1, const std::string& uid2);
    static std::optional<int> CreateGroupChat(const GroupChatData& data);

    static bool ValidateMember(const std::string& uid,int thread_id);


    //bool RemoveMember(const std::string& thread_id, const std::string& user_id);
};


#endif