#include "pch.h"
#include "CORSMiddleware.h"
#include <drogon/drogon.h>

void CORSMiddleware::invoke(const drogon::HttpRequestPtr& req,
    drogon::MiddlewareNextCallback&& nextCb,
    drogon::MiddlewareCallback&& mcb)
{
    // Add log at method entry
    //LOG_DEBUG << "CORSMiddleware: Invoked for path [" << req->getPath() << "] with method [" << req->getMethodString() << "]";

    // Handle OPTIONS preflight requests
    if (req->getMethod() == drogon::HttpMethod::Options)
    {
        LOG_DEBUG << "CORSMiddleware: Handling OPTIONS request for path [" << req->getPath() << "]";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->addHeader("Access-Control-Allow-Origin", "*"); // Allow all origins, specify specific origins in production
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        // Postman request's Access-Control-Request-Headers is "Content-Type"
        // Our middleware allows "Content-Type, Authorization", which is compatible
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        resp->addHeader("Access-Control-Max-Age", "3600"); // Preflight request cache time
        resp->setStatusCode(drogon::k204NoContent); // OPTIONS requests usually return 204
        mcb(resp); // Return directly, don't enter inner layer
        return;
    }

    // For non-OPTIONS requests, enter inner layer processing
    // Capture request path for logging
    auto originalPath = req->getPath();
    nextCb([mcb = std::move(mcb), originalPath](const drogon::HttpResponsePtr& resp) {
        //LOG_DEBUG << "CORSMiddleware: Post-processing for path [" << originalPath << "]";
        // After inner layer processing is complete, add CORS headers to response
        if (resp) {
            resp->addHeader("Access-Control-Allow-Origin", "*"); // Allow all origins
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
            resp->addHeader("Access-Control-Max-Age", "86400");
            // Can add other CORS headers as needed, e.g. Access-Control-Expose-Headers
        }
        mcb(resp); // Return response with CORS headers to upper layer
        });
}