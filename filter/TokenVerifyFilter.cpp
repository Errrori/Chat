#include "pch.h"
#include "TokenVerifyFilter.h"
#include "Common/User.h"
#include "Container.h"
#include "Service/RedisService.h"
#include "auth/TokenService.h"

void TokenVerifyFilter::doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
	drogon::FilterChainCallback&& fccb)
{
	auto token = Auth::TokenService::GetInstance().ExtractFromRequest(req);

	UserInfo info;
	std::string jti;
	if (token.empty() || !Auth::TokenService::GetInstance().Verify(
		token, Auth::TokenType::Access, info, jti))
	{
		fcb(Utils::CreateErrorResponse(401, 401, "invalid or expired access token"));
		return;
	}

	// Token is valid — info.uid is populated.
	// Try to enrich with fresh profile data from Redis cache.
	auto redis = Container::GetInstance().GetRedisService();
	if (!redis)
	{
		req->getAttributes()->insert("visitor_info", info);
		fccb();
		return;
	}

	drogon::async_run(
		[req, fcb = std::move(fcb), fccb = std::move(fccb),
		 info = std::move(info), redis]() mutable -> drogon::Task<>
		{
			auto cached = co_await redis->GetCachedUserInfo(info.getUid());
			if (!cached.getUid().empty())
			{
				req->getAttributes()->insert("visitor_info", cached);
				fccb();
				co_return;
			}
			// Cache miss: store uid-only info; controllers that need full profile
			// should call UserService::GetUserInfo(uid).
			LOG_INFO << "[TokenFilter] cache miss for uid: " << info.getUid();
			req->getAttributes()->insert("visitor_info", info);
			fccb();
		}
	);
}
