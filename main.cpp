#include <drogon/drogon.h>
#include <csignal>

void SignalHandler(int signals)
{
	drogon::app().quit();
}

void handleOptions(const drogon::HttpRequestPtr& req,
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

int main()
{
	signal(SIGINT, SignalHandler);
	signal(SIGTERM, SignalHandler);
	drogon::app().registerHandler("/auth/register",
		&handleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/auth/login",
		&handleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/db_info",
		&handleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/modify_name",
		&handleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/modify_password",
		&handleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/delete_user",
		&handleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/get_user",
		&handleOptions,
		{ drogon::Options });
	drogon::app().registerHandler("/import",
		&handleOptions,
		{ drogon::Options });

	drogon::app().setLogLevel(trantor::Logger::kDebug)
		.loadConfigFile("config.json").setThreadNum(16);
	LOG_INFO << "Server start!";
	drogon::app().run();

}