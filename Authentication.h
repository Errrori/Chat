#pragma once
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>

namespace Authentication
{
	class Controller:public drogon::HttpController<Controller>
	{
	public:
		METHOD_LIST_BEGIN
		//can add more way to get request,like url parameters
		ADD_METHOD_TO(Controller::HandleRegister, "/auth/register", drogon::Post, "CorsMiddleware");
		ADD_METHOD_TO(Controller::HandleLogin, "/auth/login", drogon::Post, "CorsMiddleware");
		METHOD_LIST_END

		void HandleRegister(const drogon::HttpRequestPtr& req,
			std::function<void(const drogon::HttpResponsePtr&)>&& callback);
		void HandleLogin(const drogon::HttpRequestPtr& req,
			std::function<void(const drogon::HttpResponsePtr&)>&& callback);
	};
}

