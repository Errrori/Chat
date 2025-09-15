#include "pch.h"
#include "AIService.h"
#include <curl/curl.h>

bool AI_MODELS::ValidateModel(const std::string& name)
{
	for (const auto& model : model_list)
	{
		if (name == model)
			return true;
	}
	return false;
}

std::optional<Json::Value> BuildParams::ToJson() const
{
	//if (user_id.empty()||messages.empty()||request_id.empty())
	//{
	//	LOG_ERROR << "user_id or message or request_id is empty";
	//	return std::nullopt;
	//}

	Json::Value data;
	data["messages"] = messages;
	data["model"] = model;
	//std::ostringstream tempStream, topPStream;
	//
	//tempStream << std::fixed << std::setprecision(2) << temperature;
	//topPStream << std::fixed << std::setprecision(2) << top_p;
	//data["temperature"] = std::stod(tempStream.str());
	//data["top_p"] = std::stod(topPStream.str());
	//data["user_id"] = user_id;
	//data["max_tokens"] = max_tokens;
//	data["request_id"] = request_id;
	//if (stream.has_value())
	//{
	//	data["stream"] = stream.value();
	//}
	//if (thinking.has_value())
	//{
	//	data["thinking"] = thinking.value();
	//}
	//if (tool.has_value())
	//{
	//	data["tools"] = tool.value();
	//}

	//data["response_format"] = resp_format;

	return data;
}

void BuildParams::SetMessages(const Json::Value& msg)
{
	messages = msg;
}

BuildParams BuildParams::FromJson(const Json::Value& data)
{
	BuildParams params;
	params.SetMessages(data["messages"]);
	//params.user_id = data["user_id"].asString();
	//params.request_id = data["request_id"].asString();
	std::ostringstream tempStream, topPStream;

	tempStream << std::fixed << std::setprecision(2) << data["temperature"].asFloat();
	topPStream << std::fixed << std::setprecision(2) << data["top_p"].asFloat();

	//if (!data.isMember("messages")||!data.isMember("response_format"))
	//{
	//	
	//}
	//params.resp_format = data["response_format"];

	//if (data.isMember("max_tokens"))
	//{
	//	params.max_tokens = data["max_tokens"].asInt();
	//}
	if (data.isMember("model"))
	{
		params.model = data["model"].asString();
	}
	if (data.isMember("tool"))
	{
		params.tool = data["tool"];
	}
	//if (data.isMember("thinking"))
	//{
	//	params.thinking = data["thinking"];
	//}
	//if (data.isMember("stream"))
	//{
	//	params.stream = data["stream"].asBool();
	//}
	return params;
}


size_t AIService::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	(static_cast<std::string*>(userp))->append(static_cast<char*>(contents), size * nmemb);
	return size * nmemb;
}

std::optional<std::string> AIService::SendRequest(const Json::Value& data,
                                                   const std::string& token, const std::string& url)
{
	CURL* curl;
	CURLcode res_code;

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		Json::StreamWriterBuilder builder;
		builder["indentation"] = "";
		const auto& req_body = Json::writeString(builder, data);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req_body.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req_body.length());

		struct curl_slist* headers = nullptr;
		headers = curl_slist_append(headers, ("Authorization: " + token).c_str());
		headers = curl_slist_append(headers, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		std::string resp;
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

		res_code = curl_easy_perform(curl);

		if (res_code != CURLE_OK)
		{
			LOG_ERROR << "perform failed: " << curl_easy_strerror(res_code);
			return std::nullopt;
		}
		LOG_INFO << "Response body: " << resp;

		curl_easy_cleanup(curl);
		curl_slist_free_all(headers);
		curl_global_cleanup();
		return resp;
	}
	return std::nullopt;
}

