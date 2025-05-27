#pragma once
#include <drogon/HttpFilter.h>
class TokenVerifyFilter:public drogon::HttpFilter<TokenVerifyFilter>
{
public:
	void doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb, drogon::FilterChainCallback&& fccb) override;
};

