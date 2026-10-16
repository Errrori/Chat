#include "pch.h"
#include "Common/ResponseHelper.h"
#include "TokenVerifyFilter.h"
#include "Common/User.h"
#include "Container.h"
#include "Service/RedisService.h"
#include "Service/UserService.h"
#include "auth/TokenService.h"
#include <memory>
#include <drogon/utils/coroutine.h>

void TokenVerifyFilter::doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
	drogon::FilterChainCallback&& fccb)
{
	auto token = Auth::TokenService::GetInstance().ExtractFromRequest(req);

	if (token.empty())
	{
		fcb(ResponseHelper::MakeResponse(401, 401, "invalid or expired access token"));
		return;
	}

	auto result = Auth::TokenService::GetInstance().Verify(token, Auth::TokenType::Access);
	if (!result)
	{
		fcb(ResponseHelper::MakeResponse(401, 401, "invalid or expired access token"));
		return;
	}

	auto redis = Container::GetInstance().GetRedisService();
	if (!redis)
	{
		// Redis 不可用时退化为纯验签，不阻塞正常请求
		req->getAttributes()->insert("uid", result->uid);
		fccb();
		return;
	}

	// 异步查询用户级吊销时间戳，再决定放行或拒绝
	auto fcb_ptr  = std::make_shared<drogon::FilterCallback>(std::move(fcb));
	auto fccb_ptr = std::make_shared<drogon::FilterChainCallback>(std::move(fccb));
	const std::string uid       = result->uid;
	const long long   issued_at = result->issued_at;

	drogon::async_run([req, redis, uid, issued_at, fcb_ptr, fccb_ptr]() -> drogon::Task<>
	{
		const bool revoked = co_await redis->IsAccessTokenRevoked(uid, issued_at);
		if (revoked)
		{
			(*fcb_ptr)(ResponseHelper::MakeResponse(401, 401, "access token has been revoked"));
		}
		else
		{
			req->getAttributes()->insert("uid", uid);
			(*fccb_ptr)();
		}
	});
}
