#include "pch.h"
#include <drogon/drogon.h>
#include <csignal>
#include "Utils.h"

using namespace Utils;

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
	drogon::app().registerHandler("/auth/register",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/auth/login",
		&HandleOptions,
		{ drogon::Options });

	drogon::app().registerHandler("/debug/db_info",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/debug/rd_info",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/debug/get_user",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/debug/import",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/debug/online_users",
		&HandleOptions,
		{ drogon::Options });

	drogon::app().registerHandler("/user/modify/username",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/user/modify/avatar",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/user/cancel",
		&HandleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/users_online",
		&HandleOptions,
		{ drogon::Options });

	drogon::app().registerHandler("/chatroom/records",
		&HandleOptions,
		{ drogon::Options });
}

int main()
{
	signal(SIGINT, SignalHandler);
	signal(SIGTERM, SignalHandler);
	
	AddOptionHandle();
	
	drogon::app().setLogLevel(trantor::Logger::kDebug)
		.loadConfigFile("config.json").setThreadNum(16);
	LOG_INFO << "Server start!";
	drogon::app().run();

}