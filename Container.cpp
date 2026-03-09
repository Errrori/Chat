#include "pch.h"
#include "Container.h"
#include <drogon/nosql/RedisClient.h>
#include <cstring>

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
#include "const.h"

// DNS 主机名解析辅助函数
namespace {
	std::string ResolveHostname(const std::string& hostname) 
	{
		// 如果已经是 IP 地址格式，直接返回
		struct in_addr test_addr;
		if (inet_pton(AF_INET, hostname.c_str(), &test_addr) == 1) {
			return hostname;
		}
		
		struct addrinfo hints, *result;
		std::memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;  // 允许 IPv4 或 IPv6
		hints.ai_socktype = SOCK_STREAM;
		
		int ret = getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
		if (ret != 0) {
			LOG_WARN << "Failed to resolve hostname: " << hostname 
			         << ", error: " << gai_strerror(ret) 
			         << ", using as-is";
			return hostname;
		}
		
		char ip_str[INET6_ADDRSTRLEN];
		if (result->ai_family == AF_INET) {
			struct sockaddr_in* ipv4 = (struct sockaddr_in*)result->ai_addr;
			inet_ntop(AF_INET, &(ipv4->sin_addr), ip_str, INET_ADDRSTRLEN);
			LOG_INFO << "Resolved " << hostname << " to " << ip_str;
		} else if (result->ai_family == AF_INET6) {
			struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)result->ai_addr;
			inet_ntop(AF_INET6, &(ipv6->sin6_addr), ip_str, INET6_ADDRSTRLEN);
			LOG_INFO << "Resolved " << hostname << " (IPv6) to " << ip_str;
		} else {
			LOG_WARN << "Unknown address family for " << hostname;
			freeaddrinfo(result);
			return hostname;
		}
		
		freeaddrinfo(result);
		return std::string(ip_str);
	}
}

Container& Container::GetInstance()
{
	static Container container;
	return container;
}

Container::Container()
{
	// ── 1. 数据库初始化（建表 + PRAGMA）──
	{
		auto db = drogon::app().getDbClient();
		for (const auto& table : DataBase::db_table_list)
		{
			try
			{
				db->execSqlSync("PRAGMA journal_mode=WAL");
				db->execSqlSync("PRAGMA busy_timeout=5000");
				db->execSqlSync(DataBase::OPEN_FK);
				db->execSqlSync(table);
			}
			catch (const std::exception& e)
			{
				LOG_ERROR << "Cannot create table: " << table << " , exception: " << e.what();
			}
		}
		LOG_INFO << "Database init success";
	}

	// ── 2. Redis 初始化（优先读环境变量，其次读 config.json）──
	{
		std::string redis_host = "127.0.0.1";
		uint16_t    redis_port = 6379;
		uint32_t    conn_num  = 2;
		
		// 优先从环境变量读取（Docker部署时使用）
		const char* env_host = std::getenv("REDIS_HOST");
		const char* env_port = std::getenv("REDIS_PORT");

		
		
		if (env_host != nullptr && strlen(env_host) > 0)
		{
			redis_host = env_host;
			LOG_INFO << "Using Redis host from environment: " << redis_host;
		}
		if (env_port != nullptr && strlen(env_port) > 0)
		{
			try
			{
				redis_port = static_cast<uint16_t>(std::stoi(env_port));
				LOG_INFO << "Using Redis port from environment: " << redis_port;
			}
			catch (const std::exception& e)
			{
				LOG_WARN << "Invalid REDIS_PORT environment variable, using default: " << e.what();
			}
		}
		
		// 如果环境变量未设置，从 config.json 读取
		if (env_host == nullptr || strlen(env_host) == 0)
		{
			try
			{
				const auto custom_config = drogon::app().getCustomConfig();
				if (custom_config.isMember("redis"))
				{
					const auto& r = custom_config["redis"];
					if (r.isMember("host")) redis_host = r["host"].asString();
					if (r.isMember("port")) redis_port = static_cast<uint16_t>(r["port"].asInt());
					if (r.isMember("connection_number")) conn_num = r["connection_number"].asUInt();
					LOG_INFO << "Using Redis config from config.json";
				}
			}
			catch (const std::exception& e)
			{
				LOG_WARN << "Failed to read redis config from file, using defaults: " << e.what();
			}
		}
		
		// 解析主机名到 IP 地址（Docker 网络或远程主机）
		std::string redis_ip = ResolveHostname(redis_host);
		
		LOG_INFO << "Connecting to Redis at " << redis_host 
		         << " (" << redis_ip << ":" << redis_port << ") "
		         << "with " << conn_num << " connections";
		
		auto redis_client = drogon::nosql::RedisClient::newRedisClient(
			trantor::InetAddress(redis_ip, redis_port), conn_num);
		redis_client->setTimeout(5.0);
		_redis_service = std::make_shared<RedisService>(redis_client);
	}

	// ── 3. Repository 创建（注入 DbClientPtr）──
	auto db = drogon::app().getDbClient();
	_user_repo         = std::make_shared<SQLiteUserRepository>(db);
	_thread_repo       = std::make_shared<SQLiteThreadRepository>(db);
	_message_repo      = std::make_shared<SQLiteMessageRepository>(db);
	_relationship_repo = std::make_shared<SQLiteRelationshipRepository>(db);

	// ── 4. Service 创建（第一阶段：构造函数注入直接依赖）──
	_user_service         = std::make_shared<UserService>(_user_repo, _redis_service);
	_thread_service       = std::make_shared<ThreadService>(_thread_repo);
	_conn_service         = std::make_shared<ConnectionService>(_redis_service);
	_message_service      = std::make_shared<MessageService>(_message_repo, _conn_service, _thread_service, _redis_service);
	_relationship_service = std::make_shared<RelationshipService>(_relationship_repo, _conn_service);

	// ── 5. 第二阶段：注入循环依赖 ──
	_relationship_service->SetUserService(_user_service);
	_message_service->SetRelationshipService(_relationship_service);

	LOG_INFO << "Container initialized: all services ready";
}
