#include "pch.h"
#include <drogon/drogon.h>
#include <atomic>
#include <csignal>
#include <curl/curl.h>
#include <filesystem>

#include "Utils.h"
#include "Container.h"
#include "Service/ConnectionService.h"
#include "Service/ClusterService.h"

using namespace Utils;

// 错误码定义
constexpr int FAIL = 400;

void SignalHandler(int signals)
{
	// 优雅退出：停止心跳、删除本节点存活标记并清理本地路由，
	// 让对端立即把消息回退到离线队列，而不是继续投递到本进程。
	static std::atomic<bool> shutting_down{false};
	if (shutting_down.exchange(true))
		return;

	auto loop = drogon::app().getLoop();
	if (!loop)
	{
		drogon::app().quit();
		return;
	}

	loop->runInLoop([]()
	{
		try
		{
			auto& container = Container::GetInstance();
			container.GetClusterService()->BeginDrain();
			container.GetConnectionService()->ClearLocalRoutes();
		}
		catch (const std::exception& e)
		{
			LOG_ERROR << "shutdown cleanup failed: " << e.what();
		}

		// 给异步 Redis 清理留出时间，再退出事件循环。
		drogon::app().getLoop()->runAfter(0.5, []() { drogon::app().quit(); });
	});
}

void AddCorsHeaders(const drogon::HttpRequestPtr& req, const drogon::HttpResponsePtr& resp)
{
	const auto origin = req->getHeader("Origin");
	if (!origin.empty())
	{
		resp->addHeader("Access-Control-Allow-Origin", origin);
		resp->addHeader("Vary", "Origin");
	}
	else
	{
		resp->addHeader("Access-Control-Allow-Origin", "*");
	}

	resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
	resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
	resp->addHeader("Access-Control-Allow-Credentials", "true");
	resp->addHeader("Access-Control-Max-Age", "3600");
}

void HandleOptions(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	auto resp = drogon::HttpResponse::newHttpResponse();
	AddCorsHeaders(req, resp);
	resp->setStatusCode(drogon::k204NoContent);
	callback(resp);
}

void AddOptionHandle()
{
	// Authentication routes
	drogon::app().registerHandler("/auth/register",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/auth/login",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/auth/refresh",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/auth/logout",
		&HandleOptions,
		{ drogon::Options });

	// Debug routes
	drogon::app().registerHandler("/debug/db_info",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/debug/online_users",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/debug/get_notifications",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/debug/thread_info",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/debug/private_thread_info",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/debug/all-records",
		&HandleOptions,
		{ drogon::Options });

	// User routes
	drogon::app().registerHandler("/user/get-user",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/user/modify/info",
		&HandleOptions,
		{ drogon::Options });

	// File routes
	drogon::app().registerHandler("/file/upload/image",
		&HandleOptions,
		{ drogon::Options });

	// Thread routes
	// Create thread
	drogon::app().registerHandler("/thread/create/private-chat",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/thread/create/group-chat",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/thread/create/ai-chat",
		&HandleOptions,
		{ drogon::Options });
	// Group member operations
	drogon::app().registerHandler("/thread/group/add-member",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/thread/group/join",
		&HandleOptions,
		{ drogon::Options });
	// Query thread info
	drogon::app().registerHandler("/thread/info/query",
		&HandleOptions,
		{ drogon::Options });
	// Records
	drogon::app().registerHandler("/thread/record/overview",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/thread/record/user",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/thread/record/ai",
		&HandleOptions,
		{ drogon::Options });
	
}

int main()
{
#ifdef _WIN32
	SetConsoleOutputCP(65001);
#endif

	curl_global_init(CURL_GLOBAL_DEFAULT);

	// 注册跨域支持
	//AddOptionHandle();

	std::string configPath = "config.json";
	for (const auto& candidate : {
		std::filesystem::path("config.json"),
		std::filesystem::path("ChatServer") / "config.json",
		std::filesystem::path("..") / "ChatServer" / "config.json"
		})
	{
		if (std::filesystem::exists(candidate))
		{
			configPath = candidate.string();
			break;
		}
	}

	std::string documentRoot = ".";  // 默认使用当前目录，如果static目录不存在的话
	for (const auto& candidate : {
		std::filesystem::path("static"),
		std::filesystem::path("ChatServer") / "static",
		std::filesystem::path("..") / "ChatServer" / "static"
		})
	{
		if (std::filesystem::exists(candidate))
		{
			documentRoot = candidate.string();
			break;
		}
	}

	try
	{
		auto& app = drogon::app().setLogLevel(trantor::Logger::kDebug)
			.loadConfigFile(configPath);
		
		if (documentRoot != ".") {  // 只有当static目录确实存在时才设置文档根目录
			app.setDocumentRoot(documentRoot);
		}
		// .setHomePage("index.html")  // 移除默认首页设置，让API路由优先处理根路径
		app.setThreadNum(16);
	}
	catch (const std::exception& e)
	{
		LOG_FATAL << "Failed to load config file: " << configPath << " , error: " << e.what();
		curl_global_cleanup();
		return 1;
	}

	//drogon::app().registerPostHandlingAdvice([](const drogon::HttpRequestPtr& req,
	//	const drogon::HttpResponsePtr& resp)
	//{
	//	AddCorsHeaders(req, resp);
	//});

	drogon::app().registerBeginningAdvice([]() {
		auto& container = Container::GetInstance();   // 强制初始化 Container（DB建表 + Redis连接 + 所有 Service）
		container.GetConnectionService()->StartHeartbeatMonitor();
		container.GetClusterService()->Start();       // 节点注册 + 心跳 + 订阅本节点通道

		// Drogon 在 run() 内部会安装它自己的 SIGTERM/SIGINT 处理器（收到即直接 quit），
		// 必须在事件循环就绪后重新接管，才能先做集群 drain 再退出。
		signal(SIGTERM, SignalHandler);
		signal(SIGINT, SignalHandler);

		LOG_INFO << "Server is ready to accept requests";
	});

	drogon::app().run();

	curl_global_cleanup();
}