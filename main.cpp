#include "pch.h"
#include <drogon/drogon.h>
#include <csignal>
#include <curl/curl.h>
#include <filesystem>

#include "Utils.h"
#include "Container.h"
#include "Service/ConnectionService.h"

using namespace Utils;

// 错误码定义
constexpr int FAIL = 400;

void SignalHandler(int signals)
{
	drogon::app().quit();
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

	signal(SIGINT, SignalHandler);
	signal(SIGTERM, SignalHandler);

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

	std::string documentRoot = "static";
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
		drogon::app().setLogLevel(trantor::Logger::kDebug)
			.loadConfigFile(configPath)
			.setDocumentRoot(documentRoot)
			.setHomePage("index.html")
			.setThreadNum(16);
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
		LOG_INFO << "Server is ready to accept requests";
	});

	drogon::app().run();

	curl_global_cleanup();
}