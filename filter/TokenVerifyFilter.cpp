#include "pch.h"
#include "TokenVerifyFilter.h"
#include "Common/User.h"
#include "Container.h"
#include "Service/RedisService.h"
#include "Service/UserService.h"
#include "auth/TokenService.h"

void TokenVerifyFilter::doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
	drogon::FilterChainCallback&& fccb)
{
	auto token = Auth::TokenService::GetInstance().ExtractFromRequest(req);

	std::string uid;
	std::string jti;
	if (token.empty() || !Auth::TokenService::GetInstance().Verify(
		token, Auth::TokenType::Access, uid, jti))
	{
		fcb(Utils::CreateErrorResponse(401, 401, "invalid or expired access token"));
		return;
	}

	req->getAttributes()->insert("uid", uid);
	fccb();
}
