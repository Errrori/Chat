#pragma once
namespace AI_MODELS
{
	const std::vector<std::string> model_list =
	{
		"glm-4.5-flash"
	};

	bool ValidateModel(const std::string& name);
}
//  --data '{
//  "model": "glm-4.5",
//  "stream" : true,
//  "thinking" : {
//	  "type": "enabled"
//  },
//  "do_sample" : true,
//  "temperature" : 0.6,
//  "top_p" : 0.95,
//  "response_format" : {
//	  "type": "text"
//  },
//  "user_id" : "211212",
//  "request_id" : "122121212",
//  "messages" : [
//  {
//	  "role": "system",
//		  "content" : "1212"
//	}
//  ] ,
// tools = [{
//	"type": "web_search",
//	"web_search" : {
//	"enable": "True",
//	"search_engine" : "search_pro",
//	"search_result" : "True",
//	"search_prompt" : "You are an experienced expert teacher who is good at using search engines to summarize key information from {search_result} and need to cite the data source at the end. Search date: April 11, 2025.",
//	"count" : "5",
//	"search_domain_filter" : "www.sohu.com",
//	"search_recency_filter" : "noLimit",
//	"content_size" : "high"
//}
//}]
//}'

namespace API_KEY
{
	const std::string key1 = "Bearer 45664786303e46ca9caa8dc3d82ce7c4.VzuV8oSl4Nb2H3GU";
}

namespace SERVICE_URL
{
	const std::string CHAT_URL = "https://open.bigmodel.cn/api/paas/v4/chat/completions";
}

//user_id,temperature,top_p,tool,max_tokens,request_id,model
struct BuildParams
{
	Json::Value messages;
	std::string model = "glm-4.5-flash";
	std::optional<Json::Value> tool;
	//Json::Value resp_format;

	std::optional<Json::Value> ToJson() const;

	void SetMessages(const Json::Value& msg);

	static BuildParams FromJson(const Json::Value& data);
};

class AIService
{
public:
	static std::optional<std::string> SendRequest(
		const Json::Value& data, const std::string& token, const std::string& url);

private:
	static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
};

