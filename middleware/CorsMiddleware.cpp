#include "pch.h"
#include "CORSMiddleware.h"
#include <drogon/drogon.h>

void CORSMiddleware::doFilter(const drogon::HttpRequestPtr& req,
    drogon::FilterCallback&& fcb,
    drogon::FilterChainCallback&& fccb)
{
    if (req->getMethod() == drogon::HttpMethod::Options)
    {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        resp->addHeader("Access-Control-Max-Age", "3600");
        resp->setStatusCode(drogon::k204NoContent);
        fcb(resp);
        return;
    }

    fccb();
}