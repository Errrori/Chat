#include "pch.h"
#include "SQLiteMessageRepository.h"

#include "DbAccessor.h"
#include "Common/ChatMessage.h"
#include "models/Messages.h"
#include "models/AiContext.h"
#include "models/PrivateChats.h"
#include "models/GroupChats.h"
#include "models/GroupMembers.h"
#include "Common/AIMessage.h"

using namespace drogon::orm;
using Messages = drogon_model::sqlite3::Messages;
using AiContext = drogon_model::sqlite3::AiContext;
using PrivateChats = drogon_model::sqlite3::PrivateChats;
using GroupChats = drogon_model::sqlite3::GroupChats;
using GroupMembers = drogon_model::sqlite3::GroupMembers;


//�����߼�����Service�㣬��Ҫ��鷢�����Ƿ��ڸ�thread��
//����ֵΪmessage_id

drogon::Task<int64_t> SQLiteMessageRepository::RecordUserMessage(const ChatMessage& message)
{
	try
	{
		drogon_model::sqlite3::Messages db_message;

		//message_id is auto-incremented in the database, so do not set it

		const auto& content = message.getContent();
		const auto& attachment = message.getAttachment();

		if (!content.empty())
			db_message.setContent(content);

		if (!attachment.empty()) {
			Json::StreamWriterBuilder builder;
			builder["indentation"] = "";
			std::string attachmentStr = Json::writeString(builder, attachment);
			db_message.setAttachment(attachmentStr);
		}

		if (attachment.empty() && content.empty())
			throw std::invalid_argument("content and attachment are both empty");

		db_message.setSenderUid(message.getSenderUid());
		db_message.setSenderName(message.getSenderName());
		db_message.setSenderAvatar(message.getSenderAvatar());
		db_message.setCreateTime(message.getCreateTime());
		db_message.setUpdateTime(message.getUpdateTime());
		db_message.setThreadId(message.getThreadId());

		CoroMapper<Messages> mapper(DbAccessor::GetDbClient());
		const auto& coro_msg = co_await mapper.insert(db_message);
		co_return coro_msg.getValueOfMessageId();
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception: " << e.what();
		throw;
	}
}


drogon::Task<> SQLiteMessageRepository::RecordAIMessage(const AIMessage& message)
{
	try
	{
		if (message.getMessageId().empty())
			throw std::invalid_argument("message id is empty");
		
		const auto& content = message.getContent();
		const auto& attachment = message.getAttachment();
		const auto& reasoning_content = message.getReasoningContent();

		if (content.empty()&&reasoning_content.empty())
		{
			throw std::invalid_argument("message id is empty");
		}
		drogon_model::sqlite3::AiContext db_message;
		if (!content.empty())
			db_message.setContent(content);
		if (!reasoning_content.empty())
			db_message.setReasoningContent(message.getReasoningContent());
		if (attachment.has_value()&&!attachment.value().empty()) {
			Json::StreamWriterBuilder builder;
			builder["indentation"] = "";
			std::string attachmentStr = Json::writeString(builder, attachment.value());
			db_message.setAttachment(attachmentStr);
		}

		db_message.setMessageId(message.getMessageId());
		db_message.setThreadId(static_cast<int64_t>(message.getThreadId()));
		db_message.setRole(AIMessage::RoleToString(message.getRole()));
		db_message.setCreatedTime(message.getCreatedTime());

		CoroMapper<AiContext> mapper(DbAccessor::GetDbClient());
		co_await mapper.insert(db_message);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception: " << e.what();
		throw;
	}
}

drogon::Task<Json::Value> SQLiteMessageRepository::GetMessageRecords(int thread_id, int64_t existed_id, int num)
{
	try
	{
		CoroMapper<Messages> mapper(DbAccessor::GetDbClient());
		Criteria criteria;
		if (existed_id!=0)
			criteria = Criteria(Messages::Cols::_thread_id, CompareOperator::EQ, thread_id)
			&& Criteria(Messages::Cols::_message_id, CompareOperator::GT, existed_id);
		else
			criteria = Criteria(Messages::Cols::_thread_id, CompareOperator::EQ, thread_id);

		auto records = co_await mapper.limit(num).findBy(criteria);

		Json::Value json_records;

		for (const auto& record : records)
			json_records.append(record.toJson());

		co_return json_records;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception: " << e.what();
		throw;
	}catch (const drogon::orm::DrogonDbException& e)
	{
		LOG_ERROR << "exception :" << e.base().what();
		throw;
	}
}


drogon::Task<Json::Value> SQLiteMessageRepository::GetAIContext(int thread_id, int64_t timestamp)
{
	try
	{
		CoroMapper<AiContext> mapper(DbAccessor::GetDbClient());

		Criteria criteria(Criteria(AiContext::Cols::_thread_id, CompareOperator::EQ, thread_id)
			&& Criteria(AiContext::Cols::_created_time, CompareOperator::GT, timestamp));

		auto context = co_await mapper.limit(20).findBy(criteria);

		Json::Value json_records;

		for (const auto& record : context)
			json_records.append(record.toJson());

		co_return json_records;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception: " << e.what();
		throw;
	}
	
}

drogon::Task<Json::Value> SQLiteMessageRepository::GetChatOverviews(int64_t existing_id, const std::string& uid)
{
	try {

		if (existing_id < 0 || uid.empty())
			throw std::invalid_argument("Invalid parameters");

		// 1. ��ȡ�û���������лỰID
		std::vector<long long> all_thread_ids;
		std::unordered_map<long long, std::string> thread_types; // ��¼�Ự����

		// ��ȡ˽�ĻỰ
		CoroMapper<drogon_model::sqlite3::PrivateChats> private_mapper(DbAccessor::GetDbClient());
		Criteria private_criteria(
			Criteria(PrivateChats::Cols::_uid1, CompareOperator::EQ, uid) ||
			Criteria(PrivateChats::Cols::_uid2, CompareOperator::EQ, uid)
		);
		auto private_chats = co_await private_mapper.findBy(private_criteria);

		// ��ȡȺ�ĻỰ
		CoroMapper<GroupMembers> group_mapper(DbAccessor::GetDbClient());
		Criteria group_criteria(Criteria(GroupMembers::Cols::_user_uid, CompareOperator::EQ, uid));
		auto groups = co_await group_mapper.findBy(group_criteria);

		// Ԥ�����������ռ�����thread_id
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
			co_return Json::Value(Json::arrayValue); // ���ؿ�����
		}

		// 2. ʹ�õ���SQL��ѯ��ȡ�����������
		auto db_client = DbAccessor::GetDbClient();

		// ����IN������ʵ��ֵ
		std::string thread_ids_str;
		for (size_t i = 0; i < all_thread_ids.size(); ++i) {
			if (i > 0) thread_ids_str += ",";
			thread_ids_str += std::to_string(all_thread_ids[i]);
		}

		// �Ż���SQL��һ�β�ѯ��ȡÿ��thread��ͳ����Ϣ��������Ϣ
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

		// ִ�в�ѯ - ʹ�ò���չ����ʽ
		// ע�⣺Drogon��execSqlSync��֧��vector��������Ҫ�������
		// ����������Ҫ���¹�����ѯ�Ա��⶯̬������������
		// ��ʱʹ�ü򻯵Ĳ�ѯ��ʽ
		const auto& result = db_client->execSqlSync(sql);

		// 3. һ���Ի�ȡ����thread����ϸ��Ϣ
		std::unordered_map<long long, Json::Value> private_infos;
		std::unordered_map<long long, Json::Value> group_infos;

		// ������ȡ˽����Ϣ
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

		// ������ȡȺ����Ϣ
		if (!groups.empty()) {
			std::vector<long long> group_thread_ids;
			for (const auto& group : groups) {
				group_thread_ids.push_back(group.getValueOfThreadId());
			}

			// ʹ��IN��ѯ������ȡȺ������
			Mapper<GroupChats> group_info_mapper(DbAccessor::GetDbClient());
			Criteria group_info_criteria(GroupChats::Cols::_thread_id, CompareOperator::In, group_thread_ids);
			const auto& group_details = group_info_mapper.findBy(group_info_criteria);

			for (const auto& detail : group_details) {
				Json::Value info = detail.toJson();
				info["type"] = "group";
				group_infos[detail.getValueOfThreadId()] = info;
			}
		}

		// 4. �������ؽ��
		Json::Value overviews(Json::arrayValue);

		for (const auto& row : result) {
			Json::Value overview;
			long long thread_id = row["thread_id"].as<long long>();

			// ���û�����Ϣ
            overview["thread_id"] = static_cast<Json::Value::Int64>(thread_id);
			overview["unread_count"] = row["unread_count"].as<int>();

			// ����thread��ϸ��Ϣ
			if (thread_types[thread_id] == "private") {
				overview["thread_info"] = private_infos[thread_id];
			}
			else {
				overview["thread_info"] = group_infos[thread_id];
			}

			// ����������Ϣ����
			if (!row["message_id"].isNull()) {
				Json::Value latest_message;
                latest_message["message_id"] = static_cast<Json::Value::Int64>(row["message_id"].as<long long>());
				latest_message["sender_uid"] = row["sender_uid"].as<std::string>();
				latest_message["sender_name"] = row["sender_name"].as<std::string>();
				latest_message["sender_avatar"] = row["sender_avatar"].as<std::string>();
				latest_message["content"] = row["content"].as<std::string>();
				latest_message["attachment"] = row["attachment"].as<std::string>();
                latest_message["thread_id"] = static_cast<Json::Value::Int64>(thread_id);
				latest_message["create_time"] = row["create_time"].as<std::string>();
				latest_message["update_time"] = row["update_time"].as<std::string>();
				overview["latest_message"] = latest_message;
			}
			else {
				overview["latest_message"] = Json::nullValue;
			}

			overviews.append(overview);
		}

		//// 5. ����û������Ϣ�ĻỰ
		//std::unordered_set<long long> processed_threads;
		//for (const auto& row : result) {
		//	processed_threads.insert(row["thread_id"].as<long long>());
		//}

		//for (long long thread_id : all_thread_ids) {
		//	if (processed_threads.find(thread_id) == processed_threads.end()) {
		//		// ����Ựû������Ϣ
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

		co_return overviews;

	}
	catch (const std::exception& e) {
		throw;
	}
}
