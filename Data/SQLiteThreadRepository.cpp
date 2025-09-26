#include "pch.h"
#include "SQLiteThreadRepository.h"

#include "DbAccessor.h"
#include "Common/ChatThread.h"
#include "manager/ThreadManager.h"
#include "models/GroupMembers.h"
#include "models/PrivateChats.h"

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
		([private_info, promise](const std::shared_ptr<drogon::orm::Transaction>& trans)
			{
				try
				{
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
					Mapper<Threads> thread_mapper(trans);
					thread_mapper.insert(group_info->ToDbThread().value(), [trans, promise, group_info](const Threads& record)
						{
							group_info->SetThreadId(record.getValueOfThreadId());
							Mapper<GroupChats> group_mapper(trans);
							group_mapper.insert(group_info->ToDbGroupChat().value(), [promise, trans, group_info](const GroupChats& group_record)
								{
									Mapper<GroupMembers> members_mapper(trans);
									members_mapper.insert(group_info->ToDbOwner().value(), [promise](const GroupMembers& member_record)
										{
											promise->set_value(member_record.getValueOfThreadId());
										},
										[trans](const DrogonDbException& e)
										{
											trans->rollback();
											LOG_ERROR << "exception: " << e.base().what();
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
				}});
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
			case ChatThreads::ThreadType::PRIVATE:
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
			case ChatThreads::ThreadType::GROUP:
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
			case ChatThreads::ThreadType::AI:
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

std::future<bool> SQLiteThreadRepository::JoinThread()
{
	std::promise<bool> promise;
	promise.set_value(true);
	return promise.get_future();
}
