#pragma once
#include <drogon/HttpMiddleware.h>

class CorsMiddleware : public drogon::HttpMiddleware<CorsMiddleware>
{
public:
    CorsMiddleware(){};

    static constexpr bool isAutoCreation{ false };
    
    void invoke(const drogon::HttpRequestPtr &req,
                drogon::MiddlewareNextCallback &&nextCb,
                drogon::MiddlewareCallback &&mcb) override;
};


