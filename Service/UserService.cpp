#include "pch.h"
#include "UserService.h"
#include "Common/User.h"
#include "Data/IUserRepository.h"
#include "RedisService.h"

using namespace Utils::Authentication;

void UserService::UserRegister(const UserInfo& info, RespCallback&& callback) const
{
	const auto& username = info.getUsername();
	const auto& password = info.getHashedPassword();
	const auto& account = info.getAccount();

	//validate essential fields
	if (username.empty() ||
		password.empty() ||
		account.empty() ||
		!IsValidAccount(account))
	{
		LOG_ERROR << "lack of essential fields";
		callback(Utils::CreateErrorResponse(400, 400, "lack of essential fields"));
		return;
	}

	//complete user info
	UserInfo user_info{ info };
	user_info.setUid(GenerateUid());

	//find if user info is already existed(account can not repeat)

	auto result = std::async(std::launch::async, [callback = std::move(callback), this, info = std::move(user_info)]()
		{
			try
			{
				auto json_record = _user_repo->GetUserInfoByAccount(info.getAccount());
				auto record = json_record.get();
				if (record != Json::nullValue)
				{
					callback(Utils::CreateSuccessResp(400, 400, "user is already existed"));
					return;
				}
				auto result = _user_repo->AddUser(info);
				if (result.get())
					callback(Utils::CreateSuccessResp(200, 200, "user register: " + info.getUsername()));
				else
					callback(Utils::CreateSuccessResp(500, 500, "can not create user info"));
			}
			catch (const std::exception& e)
			{
				LOG_ERROR << "exception: " << e.what();
				callback(Utils::CreateSuccessResp(500, 500, "can not create user info"));
			}
		});
}

void UserService::UserLogin(const UserInfo& info, RespCallback&& callback) const
{
	if (info.getAccount().empty()|| info.getHashedPassword().empty())
	{
		callback(Utils::CreateErrorResponse(400, 400, "lack of essential fields"));
		return;
	}

	auto future = std::async(std::launch::async, [callback = std::move(callback), info, this]()
		{
		try
		{
			auto json_record = _user_repo->GetUserInfoByAccount(info.getAccount());
			const auto& record = json_record.get();
			if (record==Json::nullValue)
			{
				callback(Utils::CreateErrorResponse(400, 400, "user is not existed"));
				return;
			}
			//here is the 2nd convert, so the password is hashed twice,we should use the password that in the json data
			//but if the field "password" change , the code here should change too
			const auto& user_record = UserInfo::FromJson(record);
			const auto& stored_password = record["password"].asString();

			if (stored_password ==info.getHashedPassword())
			{
				Json::Value data;
				data["uid"] = user_record.getUid();
				data["token"] = GenerateJWT(user_record);

				// 登录成功后缓存用户信息到 Redis
				auto user_json = user_record.ToJson();
				drogon::async_run([this, uid = user_record.getUid(), user_json]() -> drogon::Task<>
				{
					co_await _redis_service->CacheUserInfo(uid, user_json);
				}());

				callback(Utils::CreateSuccessJsonResp(200, 200, "success login",data));
				return;
			}
			callback(Utils::CreateErrorResponse(400, 400, "password is not correct"));

		}
		catch (const std::exception& e)
		{
			LOG_ERROR << "exception in user login: " << e.what();
			callback(Utils::CreateErrorResponse(500,500,"server error"));
		}
		});

}

drogon::Task<UserInfo> UserService::GetUserInfo(const std::string& uid)
{
	co_return co_await _user_repo->GetUserInfo(uid);
}
 