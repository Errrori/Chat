#pragma once
class UserController :public drogon::HttpController<UserController>
{
public:
	METHOD_LIST_BEGIN
		ADD_METHOD_TO(GetUser,"/user/get-user",drogon::Get, "CORSMiddleware");
		ADD_METHOD_TO(ModifyUserInfo,"/user/modify/info",drogon::Post, "CORSMiddleware", "TokenVerifyFilter");
	METHOD_LIST_END

	drogon::Task<drogon::HttpResponsePtr> GetUser(drogon::HttpRequestPtr req);
	drogon::Task<drogon::HttpResponsePtr> ModifyUserInfo(drogon::HttpRequestPtr req);
};