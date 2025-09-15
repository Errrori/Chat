#include "pch.h"
#include <drogon/drogon.h>
#include <csignal>
#include "Utils.h"

using namespace Utils;

// 错误码定义
constexpr int FAIL = 400;

void SignalHandler(int signals)
{
	drogon::app().quit();
}

void HandleOptions(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	auto resp = drogon::HttpResponse::newHttpResponse();
	resp->addHeader("Access-Control-Allow-Origin", "*");
	resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
	resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
	resp->addHeader("Access-Control-Max-Age", "3600");
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
	drogon::app().registerHandler("/user/get_user",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/user/modify/username",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/user/modify/password",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/user/modify/info",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/user/modify/avatar",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/user/cancel",
		&HandleOptions,
		{ drogon::Options });

	// File routes
	drogon::app().registerHandler("/file/upload/image",
		&HandleOptions,
		{ drogon::Options });

	// Thread routes
	drogon::app().registerHandler("/thread/create/private",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/thread/create/group",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/thread/create/ai",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/thread/group/join",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/thread/group/invite",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/thread/group/info",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/thread/user/get_id",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/thread/record/overview",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/thread/record/get",
		&HandleOptions,
		{ drogon::Options });

}

int main()
{
	signal(SIGINT, SignalHandler);
	signal(SIGTERM, SignalHandler);

	// 注册跨域支持
	AddOptionHandle();

	drogon::app().setLogLevel(trantor::Logger::kDebug)
		.loadConfigFile("config.json").setThreadNum(16);
	LOG_INFO << "Server start!";
	drogon::app().run();
}