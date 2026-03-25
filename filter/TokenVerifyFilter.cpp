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

	if (!token.empty() )
	{
		auto result = Auth::TokenService::GetInstance().Verify(
		token, Auth::TokenType::Access);
		if(result){
			req->getAttributes()->insert("uid", result->uid);
			fccb();
			return;
		}
	}

	fcb(Utils::CreateErrorResponse(401, 401, "invalid or expired access token"));
	return;
	

	
}
