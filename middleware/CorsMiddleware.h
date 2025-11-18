#pragma once

#include <drogon/HttpMiddleware.h>

class CORSMiddleware : public drogon::HttpMiddleware<CORSMiddleware>
{
public:
    CORSMiddleware() {}

    void invoke(const drogon::HttpRequestPtr& req,
        drogon::MiddlewareNextCallback&& nextCb,
        drogon::MiddlewareCallback&& mcb) override;
};



