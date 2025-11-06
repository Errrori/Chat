#include "pch.h"
#include "SQLiteThreadRepository.h"

#include "DbAccessor.h"
#include "Common/ChatThread.h"
#include "models/GroupMembers.h"
#include "models/PrivateChats.h"
#include "models/AiChats.h"
#include "models/GroupChats.h"
#include "models/Threads.h"
#include "Common/Convert.h"

using namespace drogon::orm;
using Threads = drogon_model::sqlite3::Threads;
using PrivateChats = drogon_model::sqlite3::PrivateChats;
using GroupChats = drogon_model::sqlite3::GroupChats;
using GroupMembers = drogon_model::sqlite3::GroupMembers;
using AIChats = drogon_model::sqlite3::AiChats;


std::future<int> SQLiteThreadRepository::CreatePrivateThread(const std::shared_ptr<PrivateThread> info)
{
	auto promise = std::make_shared<std::promise<int>>();
	auto private_info = std::dynamic_pointer_cast<PrivateThread>(info);
	if (private_info->IsDataValid())
	{
		DbAccessor::GetDbClient()->newTransactionAsync
		([private_info, promise](const std::shared_ptr<Transaction>& trans)
			{
				try
				{
					if (trans == nullptr)
					{
						LOG_ERROR << "fail to create transaction";
						promise->set_exception(std::make_exception_ptr("fail to create transaction"));
						return;
					}
					Mapper<Threads> thread_mapper(trans);
					thread_mapper.insert(private_info->ToDbThread().value(), [promise, trans, private_info](const Threads& record)
						{
							auto thread_id = record.getValueOfThreadId();
							Mapper<PrivateChats> private_mapper(trans);
							private_info->SetThreadId(static_cast<int>(thread_id));
							private_mapper.insert(private_info->ToDbPrivateChat().value(), [promise, thread_id](const PrivateChats& record)
								{
									promise->set_value(static_cast<int>(thread_id));
								}, [trans, promise](const DrogonDbException& e)
									{
										LOG_ERROR << "exception: " << e.base().what();
										trans->rollback();
										promise->set_value(-1);
									});
						},
						[trans, promise](const DrogonDbException& e)
						{
							LOG_ERROR << "exception: " << e.base().what();
							trans->rollback();
							promise->set_value(-1);
						});
				}
				catch (const std::exception& e)
				{
					LOG_ERROR << "exception: " << e.what();
					trans->rollback();
					promise->set_value(-1);
				}
			});
	}
	else
	{
		promise->set_value(-1);
		LOG_INFO << "error: private thread data is not valid";
	}
	return promise->get_future();
}

std::future<int> SQLiteThreadRepository::CreateGroupThread(const std::shared_ptr<GroupThread> info)
{
	auto promise = std::make_shared<std::promise<int>>();
	auto group_info = std::dynamic_pointer_cast<GroupThread>(info);
	if (group_info->IsDataValid())
	{
		DbAccessor::GetDbClient()->newTransactionAsync
		([group_info, promise](const std::shared_ptr<drogon::orm::Transaction>& trans)
			{
				try
				{
					if (trans == nullptr)
					{
						LOG_ERROR << "fail to create transaction";
						promise->set_exception(std::make_exception_ptr("fail to create transaction"));
						return;
					}
					Mapper<Threads> thread_mapper(trans);
					thread_mapper.insert(group_info->ToDbThread().value(), [trans, promise, group_info](const Threads& record)
						{
							group_info->SetThreadId(record.getValueOfThreadId());
							Mapper<GroupChats> group_mapper(trans);
							group_mapper.insert(group_info->ToDbGroupChat().value(), [promise, trans, group_info](const GroupChats& group_record)
								{
									Mapper<GroupMembers> members_mapper(trans);
									LOG_INFO << group_info->ToDbOwner().value().toJson().toStyledString();
									members_mapper.insert(group_info->ToDbOwner().value(), [promise](const GroupMembers& member_record)
										{
											promise->set_value(member_record.getValueOfThreadId());
										},
										[trans,promise](const DrogonDbException& e)
										{
											trans->rollback();
											LOG_ERROR << "exception: " << e.base().what();
											promise->set_value(-1);
										});

								}, [trans, promise](const DrogonDbException& e)
									{
										trans->rollback();
										LOG_ERROR << "exception: " << e.base().what();
										promise->set_value(-1);
									});
						}, [trans, promise](const DrogonDbException& e)
							{
								trans->rollback();
								LOG_ERROR << "exception: " << e.base().what();
								promise->set_value(-1);
							});
				}
				catch (const std::exception& e)
				{
					trans->rollback();
					LOG_ERROR << "exception: " << e.what();
					promise->set_value(-1);
				}
			});
	}
	else
	{
		LOG_INFO << "error: group thread data is not valid";
		promise->set_value(-1);
	}

	return promise->get_future();
}

std::future<int> SQLiteThreadRepository::CreateAIThread(const std::shared_ptr<AIThread> info)
{
	auto promise = std::make_shared<std::promise<int>>();

	const auto& ai_info = std::dynamic_pointer_cast<AIThread>(info);
	if (ai_info->IsDataValid())
	{
		DbAccessor::GetDbClient()->newTransactionAsync([ai_info, promise](const std::shared_ptr<Transaction>& trans)
			{
				try
				{
					if (trans==nullptr)
					{
						LOG_ERROR << "fail to create transaction";
						promise->set_exception(std::make_exception_ptr("fail to create transaction"));
						return;
					}
					Mapper<Threads> mapper(trans);
					mapper.insert(ai_info->ToDbThread().value(), [trans, ai_info, promise](const Threads& record)
						{
							Mapper<AIChats> ai_mapper(trans);
							ai_info->SetThreadId(record.getValueOfThreadId());
							ai_mapper.insert(ai_info->ToDbAiChat().value(), [promise](const AIChats& ai_record)
								{
									promise->set_value(ai_record.getValueOfThreadId());
								},
								[promise, trans](const DrogonDbException& e)
								{
									trans->rollback();
									promise->set_value(-1);
									LOG_ERROR << "exception: " << e.base().what();
								});
						},
						[trans, promise](const DrogonDbException& e)
						{
							trans->rollback();
							promise->set_value(-1);
							LOG_INFO << "exception: " << e.base().what();
						});
				}
				catch (const std::exception& e)
				{
					trans->rollback();
					LOG_ERROR << "exception: " << e.what();
					promise->set_value(-1);
				}
			});
	}
	else
	{
		LOG_ERROR << "error: ai thread data is not valid";
		promise->set_value(-1);
	}
	return promise->get_future();
}

std::future<Json::Value> SQLiteThreadRepository::GetThreadInfo(int thread_id) noexcept(false)
{
	auto promise = std::make_shared<std::promise<Json::Value>>();
	Mapper<Threads> mapper(DbAccessor::GetDbClient());

	mapper.limit(1).findBy(Criteria(Threads::Cols::_thread_id, CompareOperator::EQ, thread_id),
		[thread_id, promise](const std::vector<Threads>& t)
		{
			switch (ThreadTypeConvert::ToType(t[0].getValueOfType()))
			{
			case ChatThread::ThreadType::PRIVATE:
			{
				Mapper<PrivateChats> private_mapper(DbAccessor::GetDbClient());
				private_mapper.limit(1).findBy(Criteria(PrivateChats::Cols::_thread_id, CompareOperator::EQ, thread_id),
					[promise](const std::vector<PrivateChats>& records)
					{
						Json::Value json_data = records[0].toJson();
						json_data["type"] = TYPE_PRIVATE_CHAT;
						promise->set_value(json_data);
					},
					[promise](const DrogonDbException& e)
					{
						promise->set_value(Json::nullValue);
					});
			}
			break;
			case ChatThread::ThreadType::GROUP:
			{
				Mapper<GroupChats> group_mapper(DbAccessor::GetDbClient());
				group_mapper.limit(1).findBy(Criteria(GroupChats::Cols::_thread_id, CompareOperator::EQ, thread_id),
					[promise](const std::vector<GroupChats>& records)
					{
						Json::Value json_data = records[0].toJson();
						json_data["type"] = TYPE_GROUP_CHAT;
						promise->set_value(json_data);
					},
					[promise](const DrogonDbException& e)
					{
						promise->set_value(Json::nullValue);
					});
			}
			break;
			case ChatThread::ThreadType::AI:
			{
				Mapper<AIChats> ai_mapper(DbAccessor::GetDbClient());
				ai_mapper.limit(1).findBy(Criteria(AIChats::Cols::_thread_id, CompareOperator::EQ, thread_id),
					[promise](const std::vector<AIChats>& records)
					{
						Json::Value json_data = records[0].toJson();
						json_data["type"] = TYPE_AI_CHAT;
						promise->set_value(json_data);
					},
					[promise](const DrogonDbException& e)
					{
						promise->set_value(Json::nullValue);
					});
			}
			break;
			default:
				LOG_ERROR << "Unknown thread type";
				promise->set_value(Json::nullValue);
				break;
			}
		},
		[promise](const DrogonDbException& e)
		{
			promise->set_value(Json::nullValue);
		});
	return promise->get_future();
}

std::future<bool> SQLiteThreadRepository::AddToThread(const MemberData& member)
{
	auto promise = std::make_shared<std::promise<bool>>();
	if (!member.IsValid())
	{
		LOG_ERROR << "error: member data is not valid";
		promise->set_value(false);
		return promise->get_future();
	}

	try
	{
		Mapper<GroupMembers> mapper(DbAccessor::GetDbClient());
		const auto& member_data = member.ToDbGroupMember();
		mapper.insert(member_data.value(), [promise](const GroupMembers& member)
			{
				promise->set_value(true);
			},
			[promise](const DrogonDbException& e)	
			{
				LOG_ERROR << "exception: " << e.base().what();
				promise->set_value(false);
			});
	}catch (const std::exception& e)
	{
		LOG_ERROR << "exception: " << e.what();
		promise->set_value(false);
	}

	return promise->get_future();

}

bool SQLiteThreadRepository::IsThreadMember(int thread_id, const std::string& uid)
{
	Mapper<Threads> mapper(DbAccessor::GetDbClient());
	const auto& members = GetThreadMember(thread_id).get();
	return std::find(members.begin(), members.end(), uid) != members.end();
}

std::future<std::vector<std::string>> SQLiteThreadRepository::GetThreadMember(
	int thread_id)
{
	auto promise = std::make_shared<std::promise<std::vector<std::string>>>();
	Mapper<Threads> mapper(DbAccessor::GetDbClient());
	mapper.limit(1).findBy(Criteria(Threads::Cols::_thread_id, CompareOperator::EQ, thread_id),
		[thread_id,promise](const std::vector<Threads>& records)
		{
			try
			{
				auto thread_type = ThreadTypeConvert::ToType(records[0].getValueOfType());
				switch (thread_type)
				{
				case ChatThread::ThreadType::PRIVATE:
				{
					Mapper<PrivateChats> private_mapper(DbAccessor::GetDbClient());
					const auto& members = private_mapper.limit(1).
						findBy(Criteria(PrivateChats::Cols::_thread_id, CompareOperator::EQ, thread_id));
					if (!members.empty())
						promise->set_value({ members[0].getValueOfUid1(),members[0].getValueOfUid2() });
					else
						promise->set_exception(std::make_exception_ptr(
							std::runtime_error("can not find thread member,id: " + std::to_string(thread_id))));
				}
				break;
				case ChatThread::ThreadType::GROUP:
				{
					Mapper<GroupMembers> group_mapper(DbAccessor::GetDbClient());
					const auto& members = group_mapper.
						findBy(Criteria(GroupMembers::Cols::_thread_id, CompareOperator::EQ, thread_id));
					std::vector<std::string> user_list;
					user_list.reserve(members.size());
					for (const auto& member : members)
					{
						user_list.emplace_back(member.getValueOfUserUid());
					}
					promise->set_value(std::move(user_list));
				}
				break;
				case ChatThread::ThreadType::AI:
				{
					Mapper<AIChats> ai_mapper(DbAccessor::GetDbClient());
					const auto& members = ai_mapper.limit(1).
						findBy(Criteria(AIChats::Cols::_thread_id, CompareOperator::EQ, thread_id));
					if (!members.empty())
						promise->set_value({ members[0].getValueOfCreatorUid()});
					else
						promise->set_exception(std::make_exception_ptr(
							std::runtime_error("can not find thread member,id: " + std::to_string(thread_id))));
				}
				break;
				default:
					promise->set_exception(std::make_exception_ptr(std::invalid_argument("error thread type")));
					LOG_ERROR << "error thread type";
					break;
				}

			}catch (const std::exception& e)
			{
				LOG_ERROR << "exception: " << e.what();
				promise->set_exception(std::make_exception_ptr(e));
			}
		},
		[promise](const DrogonDbException& e)
		{
			LOG_ERROR << "db exception: " << e.base().what();
			promise->set_exception(std::make_exception_ptr(e.base()));
		});

	return promise->get_future();
}

std::future<ChatThread::ThreadType> SQLiteThreadRepository::GetThreadType(int thread_id) noexcept
{
	Mapper<Threads> mapper(DbAccessor::GetDbClient());
	auto promise = std::make_shared<std::promise<ChatThread::ThreadType>>();
	mapper.limit(1).findBy(Criteria(Threads::Cols::_thread_id, CompareOperator::EQ, thread_id),
		[promise](const std::vector<Threads>& records)
		{
			if (records.empty())
			{
				promise->set_value(ChatThread::ThreadType::UNKNOWN);
				return;
			}
			promise->set_value(ThreadTypeConvert::ToType(records[0].getValueOfType()));
		},
		[promise](const DrogonDbException& e)
		{
			LOG_ERROR << "exception: " << e.base().what();
			promise->set_value(ChatThread::ThreadType::UNKNOWN);
		});
	return promise->get_future();
}
