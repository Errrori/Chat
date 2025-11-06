#pragma once
#include "ChatThread.h"

namespace ThreadTypeConvert
{
    const std::unordered_map<int, ChatThread::ThreadType> val_to_thread_map = {
        {TYPE_GROUP_CHAT, ChatThread::ThreadType::GROUP},
        {TYPE_AI_CHAT, ChatThread::ThreadType::AI},
        {TYPE_PRIVATE_CHAT, ChatThread::ThreadType::PRIVATE}
    };

    // 从枚举到整数的映射
    const std::unordered_map<ChatThread::ThreadType, int> thread_to_val_map = {
        {ChatThread::ThreadType::GROUP, TYPE_GROUP_CHAT},
        {ChatThread::ThreadType::AI, TYPE_AI_CHAT},
        {ChatThread::ThreadType::PRIVATE, TYPE_PRIVATE_CHAT}
    };

    inline int ToVal(ChatThread::ThreadType type)
    {
        if (type != ChatThread::ThreadType::UNKNOWN)
            return thread_to_val_map.at(type);
        return -1;
    }

    inline ChatThread::ThreadType ToType(int val)
    {
        auto it = val_to_thread_map.find(val);
        if (it != val_to_thread_map.end())
            return it->second;

        return ChatThread::ThreadType::UNKNOWN;
    }
}


namespace GroupRoleConvert
{
    const std::unordered_map<int, MemberData::Role> val_to_role_map = {
        {GroupConstant::member, MemberData::Role::member},
        {GroupConstant::admin, MemberData::Role::admin},
        {GroupConstant::owner, MemberData::Role::owner}
    };

    const std::unordered_map<MemberData::Role, int> role_to_val_map = {
        {MemberData::Role::member, GroupConstant::member},
        {MemberData::Role::admin, GroupConstant::admin},
        {MemberData::Role::owner, GroupConstant::owner}
    };

    inline int ToVal(MemberData::Role role)
    {
        if (role != MemberData::Role::Unknown)
            return role_to_val_map.at(role);
        return -1;
    }

    inline MemberData::Role ToRole(int val)
    {
        auto it = val_to_role_map.find(val);
        if (it != val_to_role_map.end())
            return it->second;
        return MemberData::Role::Unknown;
    }

};