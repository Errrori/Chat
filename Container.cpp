#include "pch.h"
#include "Container.h"
#include <drogon/nosql/RedisClient.h>

#include "Data/SQLiteMessageRepository.h"
#include "Data/SQLiteThreadRepository.h"
#include "Data/SQLiteUserRepository.h"
#include "Data/SQLiteRelationshipRepository.h"
#include "Service/ThreadService.h"
#include "Service/UserService.h"
#include "Service/MessageService.h"
#include "Service/ConnectionService.h"
#include "Service/RelationshipService.h"
#include "Service/RedisService.h"
#include "controllers/ChatController.h"

Container& Container::GetInstance()
{
	static Container container;
	return container;
}

Container::Container()
{
	// ── Redis 初始化（读 config.json 中的 redis 节点）──
	{
		std::string redis_host = "127.0.0.1";
		uint16_t    redis_port = 6379;
		uint32_t    conn_num  = 2;
		try
		{
			const auto custom_config = drogon::app().getCustomConfig();
			if (custom_config.isMember("redis"))
			{
				const auto& r = custom_config["redis"];
				if (r.isMember("host")) redis_host = r["host"].asString();
				if (r.isMember("port")) redis_port = static_cast<uint16_t>(r["port"].asInt());
				if (r.isMember("connection_number")) conn_num = r["connection_number"].asUInt();
			}
		}
		catch (const std::exception& e)
		{
			LOG_WARN << "Failed to read redis config, using defaults: " << e.what();
		}
		LOG_INFO << "Connecting to Redis at " << redis_host << ":" << redis_port;
		auto redis_client = drogon::nosql::RedisClient::newRedisClient(
			trantor::InetAddress(redis_host, redis_port), conn_num);
		_redis_service = std::make_shared<RedisService>(redis_client);
	}

	_user_repo = std::make_shared<SQLiteUserRepository>();
	_user_service = std::make_shared<UserService>(_user_repo, _redis_service);
	_thread_repo = std::make_shared<SQLiteThreadRepository>();
	_thread_service = std::make_shared<ThreadService>(_thread_repo);
	_message_repo = std::make_shared<SQLiteMessageRepository>();
	_conn_service = std::make_shared<ConnectionService>(_redis_service);
	_message_service = std::make_shared<MessageService>(_message_repo, _conn_service, _thread_service, _redis_service);
	
	_relationship_repo = std::make_shared<SQLiteRelationshipRepository>();
	_relationship_service = std::make_shared<RelationshipService>(_relationship_repo, _conn_service);
}
