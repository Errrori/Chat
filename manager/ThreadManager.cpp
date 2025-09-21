#include "pch.h"
#include "ThreadManager.h"
#include "models/PrivateChats.h"
#include "models/Threads.h"
#include "models/GroupChats.h"
#include "models/GroupMembers.h"
#include "models/AiChats.h"
#include <unordered_map>

using namespace drogon::orm;
using Threads = drogon_model::sqlite3::Threads;
using GroupMembers = drogon_model::sqlite3::GroupMembers;
using PrivateChats = drogon_model::sqlite3::PrivateChats;
using GroupChats = drogon_model::sqlite3::GroupChats;
using AIChats = drogon_model::sqlite3::AiChats;
using Messages = drogon_model::sqlite3::Messages;

constexpr int CONTEXT_LEN = 20;
constexpr int MAX_TOKENS = 2048;


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
	Criteria type_criteria(Criteria(Threads::Cols::_thread_id, CompareOperator::EQ, thread_id));
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
		Criteria threads_criteria(Criteria(Threads::Cols::_thread_id,CompareOperator::EQ,thread_id)
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
	Criteria threads_criteria(Criteria(Threads::Cols::_thread_id,CompareOperator::EQ,thread_id)
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

	// Check if group chat info is found
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
	group_data["group_info"] = group_info[0].toJson();

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

	const auto& thread_id = thread_ret.getValueOfThreadId();
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
		const auto& thread_id = static_cast<int>(result.getValueOfThreadId());

		LOG_INFO << "Created thread with id: " << thread_id;

		// Use SQL insert directly to avoid ORM auto-increment key limitations
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

std::optional<int> ThreadManager::CreateAIThread(const std::string& name, const std::string& init_settings, const std::string& creator_uid)
{
	try
	{
		Mapper<Threads> threads_mapper{ DatabaseManager::GetDbClient() };
		Threads thread;
		thread.setType(AI_TYPE);
		const auto& thread_ret = threads_mapper.insertFuture(thread).get();
		const auto& thread_id = static_cast<int>(thread_ret.getValueOfThreadId());

		Mapper<AIChats> ai_mapper{ DatabaseManager::GetDbClient() };
		AIChats ai_chat;
		ai_chat.setThreadId(thread_id);
		ai_chat.setName(name);
		ai_chat.setInitSettings(init_settings);
		ai_chat.setCreatorUid(creator_uid);
		const auto& result = ai_mapper.insertFuture(ai_chat).get();
		//here need to check result
		LOG_INFO << "create new ai :" << result.toJson().toStyledString();
		return thread_id;

	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "CreateAIThread error: " << e.what();
		return std::nullopt;
	}
}

std::optional<drogon_model::sqlite3::Messages>  ThreadManager::PushChatRecords(const drogon_model::sqlite3::Messages& messages)
{
	try
	{
		Mapper<Messages> mapper{ DatabaseManager::GetDbClient() };
		std::promise<Messages> records;

		mapper.insert(messages, [&records](const Messages& message)
			{
				records.set_value(message);
				LOG_INFO << "Insert new message successful, message: " << message.toJson().toStyledString();
			},
			[&records](const DrogonDbException& e)
			{
				LOG_ERROR << "Exception to insert message: " << e.base().what();
			});

		return records.get_future().get();
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "PushChatRecords error: " << e.what();
	}
}

bool ThreadManager::ValidateMember(const std::string& uid, int thread_id)
{
	Mapper<Threads> threads_mapper{ DatabaseManager::GetDbClient() };
	Criteria threads_criteria(Criteria(Threads::Cols::_thread_id, CompareOperator::EQ, thread_id));
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
	else if (type==AI_TYPE)
	{
		Mapper<AIChats> ai_mapper{ DatabaseManager::GetDbClient() };
		Criteria ai_criteria(Criteria(AIChats::Cols::_thread_id, CompareOperator::EQ, thread_id)
			&& Criteria(AIChats::Cols::_creator_uid, CompareOperator::EQ, uid));
		const auto& ai_chats = ai_mapper.findBy(ai_criteria);
		if (ai_chats.empty())
		{
			LOG_ERROR << "can not find ai chat with thread id: " << thread_id;
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

	// 1. Get private chats the user participates in
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

		// Get other user info
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

	// 2. Get group chats the user participates in
	Mapper<GroupMembers> group_mapper(DatabaseManager::GetDbClient());
	Criteria group_criteria(Criteria(GroupMembers::Cols::_user_uid, CompareOperator::EQ, uid));
	const auto& group_members = group_mapper.findBy(group_criteria);

	for (const auto& group_member : group_members)
	{
		int thread_id = static_cast<int>(group_member.getValueOfThreadId());

		// Get group chat info
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

	// 1. Get private chat thread_ids the user participates in
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

	// 2. Get group chat thread_ids the user participates in
	Mapper<GroupMembers> group_mapper(DatabaseManager::GetDbClient());
	Criteria group_criteria(Criteria(GroupMembers::Cols::_user_uid, CompareOperator::EQ, uid));
	const auto& group_members = group_mapper.findBy(group_criteria);

	for (const auto& group_member : group_members)
	{
		thread_ids.append(static_cast<Json::Int64>(group_member.getValueOfThreadId()));
	}

	return thread_ids;
}

//before call this function, need to validate the thread id
Json::Value ThreadManager::GetAIChatContext(int thread_id)
{
	Mapper<Messages> message_mapper{ DatabaseManager::GetDbClient() };
	Criteria message_criteria(Criteria(Messages::Cols::_thread_id, CompareOperator::EQ, thread_id));
	const auto& records = message_mapper.limit(CONTEXT_LEN).findBy(message_criteria);

	try
	{
		Json::Reader reader;
		Json::Value context(Json::arrayValue);
		for (const auto& record : records)
		{
			const auto& content = record.getValueOfContent();
			LOG_INFO << "record: " << content;
			Json::Value root;
			if (reader.parse(content, root))
			{
				Json::Value json_record;
				json_record["content"] = root["content"].asString();
				json_record["role"] = root["role"].asString();
				context.append(json_record);

				//consider to handle empty and error message
			}
			else
			{
				LOG_ERROR << "can not parse record: " << content;
			}
		}

		LOG_INFO << "get ai chat context (filtered): " << context.toStyledString();

		return context;

	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception :"<<e.what();
		return Json::nullValue;
	}
	
}

std::optional<int> ThreadManager::GetThreadType(int thread_id)
{
	Mapper<Threads> threads_mapper{ DatabaseManager::GetDbClient() };
	Criteria threads_criteria(Criteria(Threads::Cols::_thread_id, CompareOperator::EQ, thread_id));
	const auto& threads = threads_mapper.findBy(threads_criteria);
	if (threads.size() != 1)
	{
		LOG_ERROR << "can not find thread with id: " << thread_id;
		return std::nullopt;
	}

	return threads[0].getValueOfType();
}

std::optional<Json::Value> ThreadManager::GetOverviewRecord(long long existing_id, const std::string& uid)
{
	try {

		if (existing_id < 0 || uid.empty()) {
			LOG_ERROR << "Invalid parameters";
			return std::nullopt;
		}
		// 1. 获取用户参与的所有会话ID
		std::vector<long long> all_thread_ids;
		std::unordered_map<long long, std::string> thread_types; // 记录会话类型

		// 获取私聊会话
		Mapper<PrivateChats> private_mapper(DatabaseManager::GetDbClient());
		Criteria private_criteria(
			Criteria(PrivateChats::Cols::_uid1, CompareOperator::EQ, uid) ||
			Criteria(PrivateChats::Cols::_uid2, CompareOperator::EQ, uid)
		);
		const auto& private_chats = private_mapper.findBy(private_criteria);

		// 获取群聊会话
		Mapper<GroupMembers> group_mapper(DatabaseManager::GetDbClient());
		Criteria group_criteria(Criteria(GroupMembers::Cols::_user_uid, CompareOperator::EQ, uid));
		const auto& groups = group_mapper.findBy(group_criteria);

		// 预分配容量并收集所有thread_id
		all_thread_ids.reserve(private_chats.size() + groups.size());

		for (const auto& private_chat : private_chats) {
			long long thread_id = private_chat.getValueOfThreadId();
			all_thread_ids.push_back(thread_id);
			thread_types[thread_id] = "private";
		}

		for (const auto& group : groups) {
			long long thread_id = group.getValueOfThreadId();
			all_thread_ids.push_back(thread_id);
			thread_types[thread_id] = "group";
		}

		if (all_thread_ids.empty()) {
			LOG_INFO << "user do not have any threads";
			return Json::Value(Json::arrayValue); // 返回空数组
		}

		// 2. 使用单个SQL查询获取所有相关数据
		auto db_client = DatabaseManager::GetDbClient();

		// 构建IN条件的实际值
		std::string thread_ids_str;
		for (size_t i = 0; i < all_thread_ids.size(); ++i) {
			if (i > 0) thread_ids_str += ",";
			thread_ids_str += std::to_string(all_thread_ids[i]);
		}

		// 优化的SQL：一次查询获取每个thread的统计信息和最新消息
		std::string sql =
			"WITH thread_stats AS ("
			"  SELECT thread_id, "
			"         COUNT(*) as unread_count,"
			"         MAX(message_id) as latest_message_id"
			"  FROM messages "
			"  WHERE thread_id IN (" + thread_ids_str + ") "
			"    AND message_id > " + std::to_string(existing_id) + " "
			"  GROUP BY thread_id"
			") "
			"SELECT ts.thread_id, ts.unread_count, "
			"       m.message_id, m.sender_uid, m.sender_name, m.sender_avatar, "
			"       m.content, m.attachment, m.create_time, m.update_time "
			"FROM thread_stats ts "
			"LEFT JOIN messages m ON ts.latest_message_id = m.message_id "
			"ORDER BY ts.thread_id";

		// 执行查询 - 使用参数展开方式
		// 注意：Drogon的execSqlSync不支持vector参数，需要逐个传递
		// 这里我们需要重新构建查询以避免动态参数数量问题
		// 暂时使用简化的查询方式
		const auto& result = db_client->execSqlSync(sql);

		// 3. 一次性获取所有thread的详细信息
		std::unordered_map<long long, Json::Value> private_infos;
		std::unordered_map<long long, Json::Value> group_infos;

		// 批量获取私聊信息
		if (!private_chats.empty()) {
			for (const auto& chat : private_chats) {
				Json::Value info;
				info["thread_id"] = static_cast<Json::Value::Int64>(chat.getValueOfThreadId());
				info["uid1"] = chat.getValueOfUid1();
				info["uid2"] = chat.getValueOfUid2();
				info["type"] = "private";
				private_infos[chat.getValueOfThreadId()] = info;
			}
		}

		// 批量获取群聊信息
		if (!groups.empty()) {
			std::vector<long long> group_thread_ids;
			for (const auto& group : groups) {
				group_thread_ids.push_back(group.getValueOfThreadId());
			}

			// 使用IN查询批量获取群聊详情
			Mapper<GroupChats> group_info_mapper(DatabaseManager::GetDbClient());
			Criteria group_info_criteria(GroupChats::Cols::_thread_id, CompareOperator::In, group_thread_ids);
			const auto& group_details = group_info_mapper.findBy(group_info_criteria);

			for (const auto& detail : group_details) {
				Json::Value info = detail.toJson();
				info["type"] = "group";
				group_infos[detail.getValueOfThreadId()] = info;
			}
		}

		// 4. 构建返回结果
		Json::Value overviews(Json::arrayValue);

		for (const auto& row : result) {
			Json::Value overview;
			long long thread_id = row["thread_id"].as<long long>();

			// 设置基本信息
			overview["thread_id"] = thread_id;
			overview["unread_count"] = row["unread_count"].as<int>();

			// 添加thread详细信息
			if (thread_types[thread_id] == "private") {
				overview["thread_info"] = private_infos[thread_id];
			}
			else {
				overview["thread_info"] = group_infos[thread_id];
			}

			// 构建最新消息对象
			if (!row["message_id"].isNull()) {
				Json::Value latest_message;
				latest_message["message_id"] = row["message_id"].as<long long>();
				latest_message["sender_uid"] = row["sender_uid"].as<std::string>();
				latest_message["sender_name"] = row["sender_name"].as<std::string>();
				latest_message["sender_avatar"] = row["sender_avatar"].as<std::string>();
				latest_message["content"] = row["content"].as<std::string>();
				latest_message["attachment"] = row["attachment"].as<std::string>();
				latest_message["thread_id"] = thread_id;
				latest_message["create_time"] = row["create_time"].as<std::string>();
				latest_message["update_time"] = row["update_time"].as<std::string>();
				overview["latest_message"] = latest_message;
			}
			else {
				overview["latest_message"] = Json::nullValue;
			}

			overviews.append(overview);
		}

		//// 5. 处理没有新消息的会话
		//std::unordered_set<long long> processed_threads;
		//for (const auto& row : result) {
		//	processed_threads.insert(row["thread_id"].as<long long>());
		//}

		//for (long long thread_id : all_thread_ids) {
		//	if (processed_threads.find(thread_id) == processed_threads.end()) {
		//		// 这个会话没有新消息
		//		Json::Value overview;
		//		overview["thread_id"] = thread_id;
		//		overview["unread_count"] = 0;
		//		overview["latest_message"] = Json::nullValue;

		//		if (thread_types[thread_id] == "private") {
		//			overview["thread_info"] = private_infos[thread_id];
		//		}
		//		else {
		//			overview["thread_info"] = group_infos[thread_id];
		//		}

		//		overviews.append(overview);
		//	}
		//}

		return overviews;

	}
	catch (const std::exception& e) {
		LOG_ERROR << "GetOverviewRecord error: " << e.what();
		return std::nullopt;
	}
}
