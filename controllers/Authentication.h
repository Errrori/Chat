#pragma once
#include <drogon/drogon.h>



class AuthController:public drogon::HttpController<AuthController>
{
	public:
		METHOD_LIST_BEGIN
		//can add more way to get request,like url parameters
		ADD_METHOD_TO(AuthController::OnRegister, "/auth/register", drogon::Post, "CORSMiddleware");
		ADD_METHOD_TO(AuthController::OnLogin, "/auth/login", drogon::Post, "CORSMiddleware");
		METHOD_LIST_END

		drogon::Task<drogon::HttpResponsePtr> OnRegister(drogon::HttpRequestPtr req);
		drogon::Task<drogon::HttpResponsePtr> OnLogin(drogon::HttpRequestPtr req);
};
