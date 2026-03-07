#include "pch.h"
#include "TokenVerifyFilter.h"
#include "Common/User.h"
#include "Container.h"
#include "Service/RedisService.h"

void TokenVerifyFilter::doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
	drogon::FilterChainCallback&& fccb)
{
	auto token = Utils::Authentication::GetToken(req);

	UserInfo info;
	if (token.empty() || !Utils::Authentication::VerifyJWT(token, info))
	{
		fcb(Utils::CreateErrorResponse(400, 400, "can not verify token"));
		return;
	}

	// 尝试从 Redis 缓存读取完整用户信息（同步方式：起动异步任务并等待）
	try
	{
		auto redis = GET_REDIS_SERVICE;
		if (redis)
		{
			// 异步查缓存——利用 drogon 协程将同步 filter 包装成异步任务
			drogon::async_run(
				[this, req, fcb = std::move(fcb), fccb = std::move(fccb),
				 info = std::move(info), redis]() mutable -> drogon::Task<>
				{
					auto cached = co_await redis->GetCachedUserInfo(info.getUid());
					if (cached != Json::nullValue)
					{
						// cache hit: 用最新资料覆盖 JWT 中的注册时信息
						auto full_info = UserInfo::FromJson(cached);
						LOG_INFO << "[TokenFilter] cache hit for uid: " << full_info.getUid();
						req->getAttributes()->insert("visitor_info", full_info);
					}
					else
					{
						// cache miss: JWT payload 带的基础信息仍可用
						LOG_INFO << "[TokenFilter] cache miss for uid: " << info.getUid();
						req->getAttributes()->insert("visitor_info", info);
					}
					fccb();
				}()
			);
			return;
		}
	}
	catch (const std::exception& e)
	{
		LOG_WARN << "[TokenFilter] Redis lookup failed, fallback to JWT payload: " << e.what();
	}

	// 如果 Redis 不可用，降级到仅用 JWT payload
	LOG_INFO << "visitor: " << info.ToString();
	req->getAttributes()->insert("visitor_info", info);
	fccb();
}
