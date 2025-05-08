#include "CorsMiddleware.h"
#include <drogon/drogon.h>

void CorsMiddleware::invoke(const drogon::HttpRequestPtr &req,
                            drogon::MiddlewareNextCallback &&nextCb,
                            drogon::MiddlewareCallback &&mcb)
{
    // 在方法入口处添加日志
    LOG_DEBUG << "CorsMiddleware: Invoked for path [" << req->getPath() << "] with method [" << req->getMethodString() << "]";

    // 处理 OPTIONS 预检请求
    if (req->getMethod() == drogon::HttpMethod::Options)
    {
        LOG_DEBUG << "CorsMiddleware: Handling OPTIONS request for path [" << req->getPath() << "]";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->addHeader("Access-Control-Allow-Origin", "*"); // 允许所有来源，生产环境请指定具体来源
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        // Postman 请求的 Access-Control-Request-Headers 是 "Content-Type"
        // 我们的中间件允许 "Content-Type, Authorization"，这是兼容的
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization"); 
        resp->addHeader("Access-Control-Max-Age", "3600"); // 预检请求缓存时间
        resp->setStatusCode(drogon::k204NoContent); // OPTIONS 请求通常返回 204
        mcb(resp); // 直接返回，不进入内层
        return;
    }

    // 对于非 OPTIONS 请求，进入内层处理
    // 捕获请求路径用于日志
    auto originalPath = req->getPath(); 
    nextCb([mcb = std::move(mcb), originalPath](const drogon::HttpResponsePtr &resp) {
        LOG_DEBUG << "CorsMiddleware: Post-processing for path [" << originalPath << "]";
        // 内层处理完成后，为响应添加 CORS 头部
        if (resp) {
             resp->addHeader("Access-Control-Allow-Origin", "*"); // 允许所有来源
             resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
             resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
             resp->addHeader("Access-Control-Max-Age", "86400");
             // 根据需要可以添加其他 CORS 头部，例如 Access-Control-Expose-Headers
        }
        mcb(resp); // 将带有 CORS 头部的响应返回给上层
    });
}