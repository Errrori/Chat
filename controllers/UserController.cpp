#include "pch.h"
#include "UserController.h"

#include "Common/User.h"
#include "Container.h"
#include "Service/UserService.h"
#include <drogon/utils/coroutine.h>

using namespace drogon;

drogon::Task<HttpResponsePtr> UserController::GetUser(HttpRequestPtr req)
{
    auto uidOpt = req->getOptionalParameter<std::string>("uid");
    auto accountOpt = req->getOptionalParameter<std::string>("account");

    auto result = co_await Container::GetInstance().GetUserService()->GetUser(uidOpt, accountOpt);
    if (!result.ok)
        co_return Utils::CreateErrorResponse(result.http_status, result.http_status, result.message);

    co_return Utils::CreateSuccessJsonResp(200, 200, result.message, result.user.ToJson());
}

Task<HttpResponsePtr> UserController::ModifyUserInfo(drogon::HttpRequestPtr req)
{
    auto json_body = req->getJsonObject();
    if (!json_body)
        co_return Utils::CreateErrorResponse(400, 400, "request format error");

    auto result = co_await Container::GetInstance().GetUserService()->ModifyUserInfo(*json_body);
    if (!result.ok)
        co_return Utils::CreateErrorResponse(result.http_status, result.http_status, result.message);

    co_return Utils::CreateSuccessJsonResp(200, 200, result.message, result.data);
}
