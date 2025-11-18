#pragma once
#include <drogon/drogon.h>
using namespace drogon;

// CORS跨域中间件
class CorsMiddleware : public HttpMiddleware<CorsMiddleware> {
public:
    // 重写中间件处理逻辑
    void invoke(const HttpRequestPtr& req,
        MiddlewareNextCallback&& nextCb,
        MiddlewareCallback&& mcb) override
    {
        auto resp = HttpResponse::newHttpResponse();

        // 2. 设置核心跨域响应头（可根据需求调整）
        resp->addHeader("Access-Control-Allow-Origin", "*");  // 允许所有源（生产环境建议指定具体域名，如"https://xxx.com"）
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");  // 允许的HTTP方法
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");  // 允许的请求头（如自定义Token、Content-Type）
        resp->addHeader("Access-Control-Allow-Credentials", "true");  // 允许携带Cookie（需配合前端withCredentials=true）
        resp->addHeader("Access-Control-Max-Age", "86400");  // 预检请求缓存时间（24小时，减少OPTIONS请求次数）

        // 3. 处理预检请求（OPTIONS方法）：直接返回204，无需进入业务逻辑
        if (req->method() == HttpMethod::Options) {
            resp->setStatusCode(k204NoContent);  // 204 No Content（无响应体）
            mcb(resp);
            return;
        }

        // 4. 非预检请求：传递到下一个中间件/业务处理器，同时将跨域头附加到最终响应
        nextCb([resp, mcb = std::move(mcb)](const HttpResponsePtr& businessResp) {
            // 合并业务响应的头信息和跨域头（避免覆盖）
            for (const auto& header : resp->headers()) {
                businessResp->addHeader(header.first, header.second);
            }
            mcb(businessResp);
            });
    }

};