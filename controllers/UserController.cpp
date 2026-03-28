#include "pch.h"
#include "Common/ResponseHelper.h"
#include "UserController.h"

#include "Common/User.h"
#include "Container.h"
#include "Service/UserService.h"
#include <drogon/utils/coroutine.h>

#include "Service/ConnectionService.h"

drogon::Task<drogon::HttpResponsePtr> UserController::GetUserProfile(drogon::HttpRequestPtr req)
{
    auto uidOpt = req->getOptionalParameter<std::string>("uid");
    if (uidOpt.has_value() && !uidOpt->empty())
    {
        auto user = co_await Container::GetInstance().GetUserService()->GetUserProfileByUid(*uidOpt);
        if (!user.IsValid())
            co_return ResponseHelper::MakeResponse(404, 404, "User is not found");

        co_return ResponseHelper::MakeResponse(200, 200, "success to get user profile", user.ToJson());
    }

    auto accountOpt = req->getOptionalParameter<std::string>("account");
    if (accountOpt.has_value() && !accountOpt->empty())
    {
        auto user = co_await Container::GetInstance().GetUserService()->SearchUserByAccount(*accountOpt);
        if (!user.IsValid())
            co_return ResponseHelper::MakeResponse(404, 404, "User is not found");

        co_return ResponseHelper::MakeResponse(200, 200, "success to search user", user.ToJson());
    }

    co_return ResponseHelper::MakeResponse(400, 400, "uid or account can not be empty");
}

drogon::Task<drogon::HttpResponsePtr> UserController::SearchUser(drogon::HttpRequestPtr req)
{
    auto accountOpt = req->getOptionalParameter<std::string>("account");
    if (!accountOpt.has_value() || accountOpt->empty())
        co_return ResponseHelper::MakeResponse(400, 400, "account can not be empty");

    auto user = co_await Container::GetInstance().GetUserService()->SearchUserByAccount(*accountOpt);
    if (!user.IsValid())
        co_return ResponseHelper::MakeResponse(404, 404, "User is not found");

    co_return ResponseHelper::MakeResponse(200, 200, "success to search user", user.ToJson());
}

drogon::Task<drogon::HttpResponsePtr> UserController::UpdateUserProfile(drogon::HttpRequestPtr req)
{
    auto json_body = req->getJsonObject();
    if (!json_body)
        co_return ResponseHelper::MakeResponse(400, 400, "request format error");

    const auto& uid = req->getAttributes()->get<std::string>("uid");
    auto update_info = UserInfoBuilder::BuildUpdatedInfo(*json_body);
    if (!update_info.HasUpdates())
        co_return ResponseHelper::MakeResponse(400, 400, "no updatable fields provided");

    auto user = co_await Container::GetInstance().GetUserService()->UpdateUserProfile(uid, update_info);
    if (!user.IsValid())
        co_return ResponseHelper::MakeResponse(400, 400, "fail to modify user's info");

    co_return ResponseHelper::MakeResponse(200, 200, "success to modify user's info", user.ToJson());
}

drogon::Task<drogon::HttpResponsePtr> UserController::CloseUserConn(drogon::HttpRequestPtr req)
{
    auto json_body = req->getJsonObject();
    if (!json_body)
        co_return ResponseHelper::MakeResponse(400, 400, "request format error");

    const auto& uid = req->getAttributes()->get<std::string>("uid");

    Container::GetInstance().GetConnectionService()->RemoveUserConn(uid);

    co_return ResponseHelper::MakeResponse(200, 200, "success to remove",Json::nullValue);
}
