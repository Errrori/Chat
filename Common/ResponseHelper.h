#pragma once
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>
#include "const.h" // for ChatCode::Code

namespace ResponseHelper
{
    // Make a unified JSON response
    inline drogon::HttpResponsePtr MakeResponse(int statusCode, int code, const std::string& message, const Json::Value& data = Json::Value())
    {
        Json::Value response;
        response["code"] = code;
        response["message"] = message;
        if (!data.isNull()) {
            response["data"] = data;
        }
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));
        return resp;
    }

    // Make an error Json object
    inline Json::Value MakeErrorJson(const std::string& msg, ChatCode::Code code, const std::string& message_id = "")
    {
        Json::Value resp;
        resp["code"] = code;
        resp["error"] = msg;
        if (!message_id.empty()) {
            resp["message_id"] = message_id;
        }
        return resp;
    }
}
