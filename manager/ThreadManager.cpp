#include "pch.h"
#include "ThreadManager.h"
#include "models/PrivateChats.h"
#include "models/Threads.h"
#include "models/Groups.h"


using namespace drogon::orm;
using Threads = drogon_model::sqlite3::Threads;
using Groups = drogon_model::sqlite3::Groups;
using GroupMembers = drogon_model::sqlite3::GroupMembers;
using PrivateChats = drogon_model::sqlite3::PrivateChats;
using GroupChats = drogon_model::sqlite3::GroupChats;


std::optional<drogon_model::sqlite3::GroupMembers> GroupMemberData::TransToGroupMembers() const
{
	if (thread_id<0||user_uid.empty())
		return std::nullopt;
	GroupMembers group_member;
	group_member.setThreadId(thread_id);
	group_member.setUserUid(user_uid);
	if (role.has_value())
		group_member.setRole(role.value());
	return group_member;
}

std::optional<GroupMemberData> GroupMemberData::BuildFromJson(const Json::Value& data)
{
	GroupMemberData member;
	if (!data.isMember("thread_id")&& !data.isMember("user_uid"))
	{
		LOG_ERROR << "invalid json data for group member";
		return std::nullopt;
	}
	member.thread_id = data["thread_id"].asInt();
	member.user_uid = data["user_uid"].asString();
	if (data.isMember("role"))
		member.role = data["role"].asInt();
	return member;
}

std::optional<GroupChats> GroupChatData::TransToGroupChats() const
{
	if (name.empty()||description.empty())
		return std::nullopt;
		
	GroupChats group_chat;
	group_chat.setName(name);
	group_chat.setDescription(description);
	if (avatar.has_value())
		group_chat.setAvatar(avatar.value());
	return group_chat;
}

std::optional<GroupChatData> GroupChatData::BuildFromJson(const Json::Value& data)
{
	GroupChatData chat_data;
	
	if(!data.isMember("group_name")||!data.isMember("group_description"))
	{
		LOG_ERROR << "lack of essential field";
		return std::nullopt;
	}

	chat_data.name = data["group_name"].asString();
	chat_data.description = data["group_description"].asString();
	if (data.isMember("group_avatar"))
		chat_data.avatar = data["group_avatar"].asString();
	return chat_data;	
}

std::optional<std::vector<std::string>> ThreadManager::GetThreadMembersUid(int thread_id)
{
	//1.get thread type
	Mapper<Threads> type_mapper{ DatabaseManager::GetDbClient() };
	Criteria type_criteria(Criteria(Threads::Cols::_id, CompareOperator::EQ, thread_id));
	const auto& threads = type_mapper.limit(1).findBy(type_criteria);

	if (threads.size() != 1)
	{
		LOG_ERROR << "can not find thread with id: " << thread_id;
		return std::nullopt;
	}
	auto type = threads[0].getValueOfType();

	std::vector<std::string> members_uid;

	if (type == GROUP_TYPE)
	{
		Mapper<GroupMembers> group_mapper(DatabaseManager::GetDbClient());
		Criteria group_criteria(Criteria(GroupMembers::Cols::_thread_id, CompareOperator::EQ, thread_id));
		const auto& group_members = group_mapper.findBy(group_criteria);
		if (group_members.empty())
		{
			LOG_ERROR << "can not find group members with thread id: " << thread_id;
			return std::nullopt;
		}

		for (const auto& member : group_members)
		{
			members_uid.emplace_back(member.getValueOfUserUid());
		}
	}
	else if (type == PRIVATE_TYPE)
	{
		Mapper<PrivateChats> private_mapper(DatabaseManager::GetDbClient());
		Criteria private_criteria(Criteria(PrivateChats::Cols::_thread_id, CompareOperator::EQ, thread_id));
		const auto& private_chat = private_mapper.limit(1).findBy(private_criteria);
		if (private_chat.empty())
		{
			LOG_ERROR << "can not find private chat with thread id: " << thread_id;
			return std::nullopt;
		}
		members_uid.emplace_back(private_chat[0].getValueOfUid1());
		members_uid.emplace_back(private_chat[0].getValueOfUid2());
	}
	else
	{
		LOG_ERROR << "invalid thread type: " << type;
		return std::nullopt;
	}

	return members_uid;
}

bool ThreadManager::AddNewGroupMember(int thread_id, const std::string& user_id,int role)
{
	try
	{
		Mapper<Threads> threads_mapper{DatabaseManager::GetDbClient()};
		Criteria threads_criteria(Criteria(Threads::Cols::_id,CompareOperator::EQ,thread_id)
			&&Criteria(Threads::Cols::_type,CompareOperator::EQ,GROUP_TYPE));
		//check if thread is a group thread
		auto num = threads_mapper.findBy(threads_criteria).size();
		if (num!=1)
		{
			LOG_ERROR << "can not find group thread with id: " << thread_id;
			return false;
		}

		GroupMemberData member;
		member.thread_id = thread_id;
		member.user_uid = user_id;
		member.role = role;
		auto group_member = member.TransToGroupMembers();
		if (!group_member.has_value())
		{
			LOG_ERROR << "can not convert group member data to GroupMembers";
			return false;
		}
		Mapper<GroupMembers> mapper{ DatabaseManager::GetDbClient() };
		auto future = mapper.limit(1).insertFuture(group_member.value());

			auto result = future.get();
			LOG_INFO << "insert group member: " << result.toJson().toStyledString();
			return true;
	}catch (const std::exception& e)
	{
		LOG_ERROR << "can not insert group member: " << e.what();
		return false;
	}

}

std::optional<Json::Value> ThreadManager::GetGroupInfo(int thread_id)
{
    Mapper<Threads> threads_mapper{DatabaseManager::GetDbClient()};
	Criteria threads_criteria(Criteria(Threads::Cols::_id,CompareOperator::EQ,thread_id)
		&&Criteria(Threads::Cols::_type,CompareOperator::EQ,GROUP_TYPE));
	//check if thread is a group thread
	auto num = threads_mapper.findBy(threads_criteria).size();
	if (num!=1)
	{
		LOG_ERROR << "thread type is not group: " << thread_id;
		return std::nullopt;
	}
	Mapper<GroupChats> group_mapper{ DatabaseManager::GetDbClient() };
	Criteria group_criteria(Criteria(GroupChats::Cols::_thread_id, CompareOperator::EQ, thread_id));
	const auto& group_info = group_mapper.limit(1).findBy(group_criteria);

	// 检查是否找到群聊信息
	if (group_info.empty())
	{
		LOG_ERROR << "can not find group chat info with thread id: " << thread_id;
		return std::nullopt;
	}

    Mapper<GroupMembers> members_mapper{DatabaseManager::GetDbClient()};
    Criteria members_criteria(Criteria(GroupMembers::Cols::_thread_id,CompareOperator::EQ,thread_id));
    const auto& members_info = members_mapper.limit(100).findBy(members_criteria);

	Json::Value group_data;

    Json::Value json_info(Json::arrayValue);
    for(const auto& info:members_info){
        json_info.append(info.toJson());
    }
	group_data["thread_id"] = thread_id;
	group_data["member"] = json_info;
	group_data["group_info"] = group_info[0].toJson().toStyledString();

    return group_data;
}

Json::Value ThreadManager::GetThreadInfo()
{
	Json::Value thread_info;
	Mapper<Threads> thread_mapper(DatabaseManager::GetDbClient());
	auto threads = thread_mapper.findAll();
	for (const auto& thread:threads)
	{
		thread_info.append(thread.toJson());
	}
	return thread_info;
}

Json::Value ThreadManager::GetPrivateChatInfo()
{
	Json::Value info;
	Mapper<PrivateChats> private_mapper(DatabaseManager::GetDbClient());
	auto threads = private_mapper.findAll();
	for (const auto& thread : threads)
	{
		Json::Value chat_info;
		chat_info["thread_id"] = static_cast<Json::Int64>(thread.getValueOfThreadId());
		chat_info["uid1"] = thread.getValueOfUid1();
		chat_info["uid2"] = thread.getValueOfUid2();
		info.append(chat_info);
	}
	return info;
}


std::optional<int> ThreadManager::CreatePrivateThread(const std::string& uid1, const std::string& uid2)
{

	LOG_INFO << "uid1: " << uid1 << ", uid2: " << uid2;
	auto normalized_uid1 = uid1;
	auto normalized_uid2 = uid2;

	if (uid2 < uid1) std::swap(normalized_uid1, normalized_uid2);
	LOG_INFO << "normalized uid1: " << normalized_uid1 << ", normalized uid2: " << normalized_uid2;

	Mapper<PrivateChats> private_mapper{ DatabaseManager::GetDbClient() };

	Criteria criteria(Criteria(PrivateChats::Cols::_uid1, CompareOperator::EQ, normalized_uid1)
		&& Criteria(PrivateChats::Cols::_uid2,CompareOperator::EQ,normalized_uid2));
	const auto& result = private_mapper.findFutureBy(criteria).get();
	if (!result.empty())
	{
		LOG_ERROR << "thread already exists for users: " << result[0].getValueOfThreadId();
		return std::nullopt;
	}

	Mapper<Threads> threads_mapper{ DatabaseManager::GetDbClient() };
	Threads thread;
	thread.setType(PRIVATE_TYPE);
	const auto& thread_ret = threads_mapper.insertFuture(thread).get();
	LOG_INFO << "create new thread: " << thread_ret.toJson().toStyledString();

	const auto& thread_id = thread_ret.getValueOfId();
	LOG_INFO<<"thread id: "<<thread_id;
	PrivateChats private_chat;
	private_chat.setThreadId(thread_id);
	private_chat.setUid1(normalized_uid1);
	private_chat.setUid2(normalized_uid2);

	private_mapper.insertFuture(private_chat).get();

	LOG_INFO<<"insert private chat: "<<private_chat.toJson().toStyledString();;

	return thread_id;
}

std::optional<int> ThreadManager::CreateGroupChat(const GroupChatData& data)
{
	try
	{
		LOG_INFO << "Creating group chat with name: " << data.name;

		Mapper<Threads> threads_mapper{ DatabaseManager::GetDbClient() };
		Threads thread;
		thread.setType(GROUP_TYPE);
		const auto& result = threads_mapper.insertFuture(thread).get();
		const auto& thread_id = result.getValueOfId();

		LOG_INFO << "Created thread with id: " << thread_id;

		// 直接使用SQL插入，避免ORM的自动主键限制
		auto db_client = DatabaseManager::GetDbClient();
		std::string sql = "INSERT INTO group_chats (thread_id, name, description";
		std::string values = " VALUES (?, ?, ?";

		if (data.avatar.has_value()) {
			sql += ", avatar";
			values += ", ?";
		}

		sql += ")" + values + ")";

		LOG_INFO << "Executing SQL: " << sql;

		if (data.avatar.has_value()) {
			db_client->execSqlSync(sql, thread_id, data.name, data.description, data.avatar.value());
		} else {
			db_client->execSqlSync(sql, thread_id, data.name, data.description);
		}

		LOG_INFO << "Successfully inserted group chat with thread_id: " << thread_id;

		return thread_id;
	}catch (const std::exception& e)
	{
		LOG_ERROR << "CreateGroupChat error: " << e.what();
		return std::nullopt;
	}
}

bool ThreadManager::ValidateMember(const std::string& uid, int thread_id)
{
	Mapper<Threads> threads_mapper{ DatabaseManager::GetDbClient() };
	Criteria threads_criteria(Criteria(Threads::Cols::_id, CompareOperator::EQ, thread_id));
	const auto& threads = threads_mapper.findBy(threads_criteria);

	if(threads.empty())
	{
		LOG_ERROR << "can not find thread with id: " << thread_id;
		return false;
	}

	auto type = threads[0].getValueOfType();

	if (type == GROUP_TYPE){
		Mapper<GroupMembers> group_mapper{ DatabaseManager::GetDbClient() };
		Criteria group_criteria(Criteria(GroupMembers::Cols::_thread_id, CompareOperator::EQ, thread_id)
			&& Criteria(GroupMembers::Cols::_user_uid, CompareOperator::EQ, uid));
		const auto& group_members = group_mapper.findBy(group_criteria);
		if (group_members.empty())
		{
			LOG_ERROR << "can not find :"<<"uid: "<<uid<<" group members with thread id: " << thread_id;
			return false;
		}
		return true;
	}
	else if (type == PRIVATE_TYPE){
		Mapper<PrivateChats> private_mapper{ DatabaseManager::GetDbClient() };
		Criteria private_criteria(Criteria(PrivateChats::Cols::_thread_id, CompareOperator::EQ, thread_id)
			&& (Criteria(PrivateChats::Cols::_uid1, CompareOperator::EQ, uid)
				|| Criteria(PrivateChats::Cols::_uid2, CompareOperator::EQ, uid)));
		const auto& private_chats = private_mapper.findBy(private_criteria);
		if (private_chats.empty())
		{
			LOG_ERROR << "can not find private chat with thread id: " << thread_id;
			return false;
		}
		return true;
	}
	else
	{
		LOG_ERROR << "invalid thread type: " << type;
		return false;
	}
}

Json::Value ThreadManager::GetUserChatList(const std::string& uid)
{
	Json::Value chat_list(Json::arrayValue);

	// 1. 获取用户参与的私聊
	Mapper<PrivateChats> private_mapper(DatabaseManager::GetDbClient());
	Criteria private_criteria(
		Criteria(PrivateChats::Cols::_uid1, CompareOperator::EQ, uid) ||
		Criteria(PrivateChats::Cols::_uid2, CompareOperator::EQ, uid)
	);
	const auto& private_chats = private_mapper.findBy(private_criteria);

	for (const auto& private_chat : private_chats)
	{
		Json::Value chat_info;
		chat_info["thread_id"] = static_cast<Json::Int64>(private_chat.getValueOfThreadId());
		chat_info["type"] = "private";

		// 获取对方用户信息
		std::string other_uid = (private_chat.getValueOfUid1() == uid) ?
			private_chat.getValueOfUid2() : private_chat.getValueOfUid1();

		Json::Value other_user_info;
		if (DatabaseManager::GetUserInfoByUid(other_uid, other_user_info))
		{
			chat_info["chat_name"] = other_user_info["username"].asString();
			chat_info["chat_avatar"] = other_user_info["avatar"].asString();
			chat_info["other_uid"] = other_uid;
		}
		else
		{
			chat_info["chat_name"] = "Unknown User";
			chat_info["chat_avatar"] = "https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png";
			chat_info["other_uid"] = other_uid;
		}

		chat_list.append(chat_info);
	}

	// 2. 获取用户参与的群聊
	Mapper<GroupMembers> group_mapper(DatabaseManager::GetDbClient());
	Criteria group_criteria(Criteria(GroupMembers::Cols::_user_uid, CompareOperator::EQ, uid));
	const auto& group_members = group_mapper.findBy(group_criteria);

	for (const auto& group_member : group_members)
	{
		int thread_id = group_member.getValueOfThreadId();

		// 获取群聊信息
		Mapper<GroupChats> group_chat_mapper(DatabaseManager::GetDbClient());
		Criteria group_chat_criteria(Criteria(GroupChats::Cols::_thread_id, CompareOperator::EQ, thread_id));
		const auto& group_chats = group_chat_mapper.limit(1).findBy(group_chat_criteria);

		if (!group_chats.empty())
		{
			const auto& group_chat = group_chats[0];
			Json::Value chat_info;
			chat_info["thread_id"] = static_cast<Json::Int64>(thread_id);
			chat_info["type"] = "group";
			chat_info["chat_name"] = group_chat.getValueOfName();
			chat_info["chat_avatar"] = group_chat.getValueOfAvatar();
			chat_info["description"] = group_chat.getValueOfDescription();

			chat_list.append(chat_info);
		}
	}

	return chat_list;
}

Json::Value ThreadManager::GetUserThreadIds(const std::string& uid)
{
	Json::Value thread_ids(Json::arrayValue);

	// 1. 获取用户参与的私聊thread_id
	Mapper<PrivateChats> private_mapper(DatabaseManager::GetDbClient());
	Criteria private_criteria(
		Criteria(PrivateChats::Cols::_uid1, CompareOperator::EQ, uid) ||
		Criteria(PrivateChats::Cols::_uid2, CompareOperator::EQ, uid)
	);
	const auto& private_chats = private_mapper.findBy(private_criteria);

	for (const auto& private_chat : private_chats)
	{
		thread_ids.append(static_cast<Json::Int64>(private_chat.getValueOfThreadId()));
	}

	// 2. 获取用户参与的群聊thread_id
	Mapper<GroupMembers> group_mapper(DatabaseManager::GetDbClient());
	Criteria group_criteria(Criteria(GroupMembers::Cols::_user_uid, CompareOperator::EQ, uid));
	const auto& group_members = group_mapper.findBy(group_criteria);

	for (const auto& group_member : group_members)
	{
		thread_ids.append(static_cast<Json::Int64>(group_member.getValueOfThreadId()));
	}

	return thread_ids;
}