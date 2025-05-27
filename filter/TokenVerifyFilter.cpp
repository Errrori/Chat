#include "pch.h"
#include "TokenVerifyFilter.h"

void TokenVerifyFilter::doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
	drogon::FilterChainCallback&& fccb)
{
	const auto & data = req->getJsonObject();
	
	if (!data)
	{
		fcb(Utils::CreateErrorResponse(400, 400, "The request body must be in json format"));
		return;
	}

	auto token = Utils::Authentication::GetToken(req);
	Utils::UserInfo info;
	if (token.empty() || !Utils::Authentication::VerifyJWT(token, info))
	{
		fcb(Utils::CreateErrorResponse(400, 400, "can not verify token"));
		return;
	}

	req->getAttributes()->insert("visitor_info", info);
	fccb();
}
