#include "pch.h"
#include "UserController.h"

#include "Common/User.h"
#include "Container.h"
#include "Service/UserService.h"

using namespace drogon;

drogon::Task<HttpResponsePtr> UserController::GetUser(HttpRequestPtr req)
{
    auto uidOpt = req->getOptionalParameter<std::string>("uid");
    auto accountOpt = req->getOptionalParameter<std::string>("account");
    co_return co_await Container::GetInstance().GetUserService()->GetUser(uidOpt, accountOpt);
}

Task<HttpResponsePtr> UserController::ModifyUserInfo(drogon::HttpRequestPtr req)
{
    auto json_body = req->getJsonObject();
    if (!json_body)
        co_return Utils::CreateErrorResponse(400, 400, "request format error");

    co_return co_await Container::GetInstance().GetUserService()->ModifyUserInfo(*json_body);
}
