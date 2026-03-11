#pragma once

#include <drogon/HttpFilter.h>

class CORSMiddleware : public drogon::HttpFilter<CORSMiddleware>
{
public:
    CORSMiddleware() {}

    void doFilter(const drogon::HttpRequestPtr& req,
        drogon::FilterCallback&& fcb,
        drogon::FilterChainCallback&& fccb) override;
};



