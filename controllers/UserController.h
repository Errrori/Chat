#pragma once
#include <drogon/drogon.h>

class UserController :public drogon::HttpController<UserController>
{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(UserController::GetUserProfile,"/user/profile",drogon::Get);
        ADD_METHOD_TO(UserController::GetUserProfile,"/user/get-user",drogon::Get);
        ADD_METHOD_TO(UserController::SearchUser,"/user/search",drogon::Get);
        ADD_METHOD_TO(UserController::UpdateUserProfile,"/user/profile",drogon::Post, "TokenVerifyFilter");
        ADD_METHOD_TO(UserController::UpdateUserProfile,"/user/modify/info",drogon::Post, "TokenVerifyFilter");
    METHOD_LIST_END

    drogon::Task<drogon::HttpResponsePtr> GetUserProfile(drogon::HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr> SearchUser(drogon::HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr> UpdateUserProfile(drogon::HttpRequestPtr req);
};