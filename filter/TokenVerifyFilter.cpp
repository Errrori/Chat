#include "pch.h"
#include "TokenVerifyFilter.h"
#include "Common/User.h"

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

	LOG_INFO << "visitor: " << info.ToString();

	req->getAttributes()->insert("visitor_info", info);
	fccb();
}
