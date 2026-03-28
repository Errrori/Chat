#include "pch.h"
#include "PostgresThreadRepository.h"


#include "Common/ChatThread.h"
#include "models/GroupMembers.h"
#include "models/PrivateChats.h"
#include "models/AiChats.h"
#include "models/GroupChats.h"
#include "models/Threads.h"
#include "Common/Convert.h"

using namespace drogon::orm;
using Threads = drogon_model::postgres::Threads;
using PrivateChats = drogon_model::postgres::PrivateChats;
using GroupChats = drogon_model::postgres::GroupChats;
using GroupMembers = drogon_model::postgres::GroupMembers;
using AIChats = drogon_model::postgres::AiChats;


drogon::Task<int> PostgresThreadRepository::CreatePrivateThread(PrivateThread info)
{
	try
	{
		if (!info.IsDataValid())
			throw std::invalid_argument("info is not valid");

		auto trans = co_await _db->newTransactionCoro();
		if (trans == nullptr)
			throw std::runtime_error("fail to create transaction");

		auto db_thread = info.ToDbThread();

		CoroMapper<Threads> thread_mapper(trans);
		try
		{
			 auto result= co_await thread_mapper.insert(db_thread);
			 auto thread_id = result.getValueOfThreadId();
			 info.SetThreadId(thread_id);
			 LOG_INFO << "thread id: " << thread_id;
		}
		catch (const std::exception& e){
			trans->rollback();
			throw;
		}

		auto private_info = info.ToDbPrivateChat();
		if (!private_info.has_value())
		{
			trans->rollback();
			throw std::invalid_argument("can not get private thread info");
		}

		CoroMapper<PrivateChats> private_mapper(trans);
		try
		{
			co_await private_mapper.insert(private_info.value());
		}
		catch (const std::exception& e) {
			trans->rollback();
			throw;
		}
		co_return info.GetThreadId();
	}
	catch (const std::exception& e)
	{
		throw;
	}
}

drogon::Task<int> PostgresThreadRepository::CreateGroupThread(GroupThread info)
{
	try
	{
		if (!info.IsDataValid())
			throw std::invalid_argument("info is not valid");

		auto trans = co_await _db->newTransactionCoro();
		if (trans == nullptr)
			throw std::runtime_error("fail to create transaction");

		auto db_thread = info.ToDbThread();


		CoroMapper<Threads> thread_mapper(trans);
		int thread_id = -1;

		try
		{
			auto result = co_await thread_mapper.insert(db_thread);
			thread_id = result.getValueOfThreadId();
			info.SetThreadId(thread_id);
		}catch (const std::exception& e)
		{
			trans->rollback();
			throw;
		}

		auto group_info = info.ToDbGroupChat();
		if (!group_info.has_value())
		{
			trans->rollback();
			throw std::invalid_argument("can not get group thread info");
		}

		CoroMapper<GroupChats> group_mapper(trans);
		try
		{
			co_await group_mapper.insert(group_info.value());
		}
		catch (const std::exception& e)
		{
			trans->rollback();
			throw;
		}

		auto db_member = info.ToDbOwner();
		if (!db_member.has_value())
			throw std::invalid_argument("lack of essential fields");

		CoroMapper<GroupMembers> member_mapper(trans);
		try
		{
			co_await member_mapper.insert(db_member.value());
		}catch (const std::exception& e)
		{
			throw;
		}

		co_return thread_id;
	}
	catch (const std::exception& e)
	{
		throw;
	}
}

drogon::Task<int> PostgresThreadRepository::CreateAIThread(AIThread info)
{
	try
	{
		if (!info.IsDataValid())
			throw std::invalid_argument("info is not valid");

		auto trans = co_await _db->newTransactionCoro();
		if (trans == nullptr)
			throw std::runtime_error("fail to create transaction");

		auto db_thread = info.ToDbThread();
		int thread_id = -1;

		CoroMapper<Threads> thread_mapper(trans);
		try
		{
			auto result = co_await thread_mapper.insert(db_thread);
			thread_id = result.getValueOfThreadId();
			info.SetThreadId(thread_id);
		}
		catch (const std::exception& e) {
			trans->rollback();
			throw;
		}

		auto ai_info = info.ToDbAiChat();
		if (!ai_info.has_value())
		{
			trans->rollback();
			throw std::invalid_argument("can not get ai thread info");
		}

		CoroMapper<AIChats> private_mapper(trans);
		try
		{
			co_await private_mapper.insert(ai_info.value());
		}
		catch (const std::exception& e) {
			trans->rollback();
			throw;
		}
		co_return thread_id;
	}
	catch (const std::exception& e)
	{
		throw;
	}
}

drogon::Task<Json::Value> PostgresThreadRepository::GetThreadInfo(int thread_id)
{
	CoroMapper<Threads> mapper(_db);

	auto type = co_await mapper.limit(1).
		findBy(Criteria(Threads::Cols::_thread_id, CompareOperator::EQ, thread_id));

	if (type.empty())
	{
		LOG_INFO << "error thread type";
		co_return Json::nullValue;
	}

	switch (ThreadTypeConvert::ToType(type[0].getValueOfType()))
	{
	case ChatThread::ThreadType::PRIVATE:
	{
		CoroMapper<PrivateChats> private_mapper(_db);
		auto info = co_await private_mapper.limit(1).
			findBy(Criteria(PrivateChats::Cols::_thread_id, CompareOperator::EQ, thread_id));
		if (!info.empty()) {
			Json::Value json_info{ info[0].toJson() };
			json_info["type"] = TYPE_PRIVATE_CHAT;
			co_return json_info;
		}
	}
	break;
	case ChatThread::ThreadType::GROUP:
	{
		CoroMapper<GroupChats> group_mapper(_db);
		auto info = co_await group_mapper.limit(1).
			findBy(Criteria(GroupChats::Cols::_thread_id, CompareOperator::EQ, thread_id));
		if (!info.empty()) {
			Json::Value json_info{ info[0].toJson() };
			json_info["type"] = TYPE_GROUP_CHAT;
			co_return json_info;
		}
	}
	break;
	case ChatThread::ThreadType::AI:
	{
		CoroMapper<AIChats> ai_mapper(_db);
		auto info = co_await ai_mapper.limit(1).
			findBy(Criteria(AIChats::Cols::_thread_id, CompareOperator::EQ, thread_id));
		if (!info.empty()) {
			Json::Value json_info{ info[0].toJson() };
			json_info["type"] = TYPE_AI_CHAT;
			co_return json_info;
		}
	}
	break;
	default:
		LOG_ERROR << "Unknown thread type";
		throw std::invalid_argument("unknown thread type");
	}
	LOG_INFO << "can not find thread info";
	co_return Json::nullValue;
}

drogon::Task<bool> PostgresThreadRepository::AddToGroup(const MemberData& member)
{
    if (!member.IsValid())
    {
        LOG_ERROR << "error: member data is not valid";
        throw std::invalid_argument("member data is not valid");
    }

    try
    {
        CoroMapper<GroupMembers> mapper(_db);
        auto member_data = member.ToDbGroupMember();
        co_await mapper.insert(member_data.value());
        co_return true;
    }catch (const std::exception& e)
    {
        throw;
    }

}

drogon::Task<bool> PostgresThreadRepository::IsThreadMember(int thread_id, const std::string& uid)
{
	CoroMapper<Threads> mapper(_db);
	auto members = co_await GetThreadMember(thread_id);
	co_return std::find(members.begin(), members.end(), uid) != members.end();
}

drogon::Task<std::vector<std::string>> PostgresThreadRepository::GetThreadMember(
	int thread_id)
{
	try
	{
		CoroMapper<Threads> mapper(_db);

		auto type = co_await mapper.limit(1).findBy(
			Criteria(Threads::Cols::_thread_id, CompareOperator::EQ, thread_id));

		if (type.empty())
			co_return{};

		switch (ThreadTypeConvert::ToType(type[0].getValueOfType()))
		{
		case ChatThread::ThreadType::PRIVATE:
		{
			CoroMapper<PrivateChats> private_mapper(_db);
			auto members = co_await private_mapper.limit(1).
				findBy(Criteria(PrivateChats::Cols::_thread_id, CompareOperator::EQ, thread_id));
			if (!members.empty())
				co_return{ members[0].getValueOfUid1(),members[0].getValueOfUid2() };

			throw std::runtime_error("can not find thread member,id: " + std::to_string(thread_id));
		}
		case ChatThread::ThreadType::GROUP:
		{
			CoroMapper<GroupMembers> group_mapper(_db);
			auto members = co_await group_mapper.
				findBy(Criteria(GroupMembers::Cols::_thread_id, CompareOperator::EQ, thread_id));
			std::vector<std::string> user_list;
			user_list.reserve(members.size());
			for (const auto& member : members)
				user_list.emplace_back(member.getValueOfUserUid());
			co_return std::move(user_list);
		}
		case ChatThread::ThreadType::AI:
		{
			CoroMapper<AIChats> ai_mapper(_db);
			auto members = co_await ai_mapper.limit(1).
				findBy(Criteria(AIChats::Cols::_thread_id, CompareOperator::EQ, thread_id));
			if (!members.empty())
				co_return{ members[0].getValueOfCreatorUid() };
			throw std::runtime_error("can not find thread member,id: " + std::to_string(thread_id));
		}
		default:
			throw std::invalid_argument("error thread type");
		}
	}catch (const std::exception& e)
	{
		LOG_ERROR << "exception: " << e.what();
		throw;
	}
	
}

drogon::Task<ChatThread::ThreadType> PostgresThreadRepository::GetThreadType(int thread_id)
{
	try
	{
		CoroMapper<Threads> mapper(_db);
		auto type = co_await mapper.limit(1).
			findBy(Criteria(Threads::Cols::_thread_id, CompareOperator::EQ, thread_id));
		if (type.empty())
			co_return ChatThread::UNKNOWN;

		co_return ThreadTypeConvert::ToType(type[0].getValueOfType());
	}catch (const std::exception& e)
	{
		throw;
	}
}

drogon::Task<std::pair<ChatThread::ThreadType, std::vector<std::string>>>
PostgresThreadRepository::GetTypeAndMembers(int thread_id)
{
	try
	{
		CoroMapper<Threads> mapper(_db);
		auto type_rows = co_await mapper.limit(1).
			findBy(Criteria(Threads::Cols::_thread_id, CompareOperator::EQ, thread_id));

		if (type_rows.empty())
			co_return { ChatThread::UNKNOWN, {} };

		const auto thread_type = ThreadTypeConvert::ToType(type_rows[0].getValueOfType());

		switch (thread_type)
		{
		case ChatThread::ThreadType::PRIVATE:
		{
			CoroMapper<PrivateChats> private_mapper(_db);
			auto members = co_await private_mapper.limit(1).
				findBy(Criteria(PrivateChats::Cols::_thread_id, CompareOperator::EQ, thread_id));
			if (!members.empty())
				co_return { thread_type, { members[0].getValueOfUid1(), members[0].getValueOfUid2() } };
			throw std::runtime_error("can not find private thread members, id: " + std::to_string(thread_id));
		}
		case ChatThread::ThreadType::GROUP:
		{
			CoroMapper<GroupMembers> group_mapper(_db);
			auto members = co_await group_mapper.
				findBy(Criteria(GroupMembers::Cols::_thread_id, CompareOperator::EQ, thread_id));
			std::vector<std::string> user_list;
			user_list.reserve(members.size());
			for (const auto& m : members)
				user_list.emplace_back(m.getValueOfUserUid());
			co_return { thread_type, std::move(user_list) };
		}
		case ChatThread::ThreadType::AI:
		{
			CoroMapper<AIChats> ai_mapper(_db);
			auto members = co_await ai_mapper.limit(1).
				findBy(Criteria(AIChats::Cols::_thread_id, CompareOperator::EQ, thread_id));
			if (!members.empty())
				co_return { thread_type, { members[0].getValueOfCreatorUid() } };
			throw std::runtime_error("can not find AI thread members, id: " + std::to_string(thread_id));
		}
		default:
			throw std::invalid_argument("unknown thread type, id: " + std::to_string(thread_id));
		}
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception in GetThreadMemberWithType: " << e.what();
		throw;
	}
}

drogon::Task<std::pair<ChatThread::ThreadType, std::vector<std::string>>>
PostgresThreadRepository::GetMembersAndType(int thread_id)
{
	co_return co_await GetTypeAndMembers(thread_id);
}
