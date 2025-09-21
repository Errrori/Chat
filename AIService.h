#pragma once
#include <curl/curl.h>

namespace AI_MODELS
{
	const std::vector<std::string> model_list =
	{
		"glm-4.5-flash"
	};

	bool ValidateModel(const std::string& name);
}

namespace API_KEY
{
	const std::string key1 = "Bearer 45664786303e46ca9caa8dc3d82ce7c4.VzuV8oSl4Nb2H3GU";
}

namespace SERVICE_URL
{
	const std::string CHAT_URL = "https://open.bigmodel.cn/api/paas/v4/chat/completions";
}

static const std::string terminator = "[DONE]";

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
class StreamCallback {
public:
	virtual void onChunk(const std::string& chunk) = 0;
	virtual void onComplete() = 0;
	virtual void onError(const std::string& error) = 0;
};

class ChatStreamCallback : public StreamCallback {
private:
    drogon::WebSocketConnectionPtr _conn;
    int _thread_id;
    std::string _accumulated_content;

public:
    ChatStreamCallback(drogon::WebSocketConnectionPtr conn, int thread_id)
        : _conn(std::move(conn)), _thread_id(thread_id) {
    }

	std::string GetContent() const;
    void onChunk(const std::string& chunk) override;
	void onComplete() override;
	void onError(const std::string& error) override;
};

struct StreamContext {
    std::shared_ptr<StreamCallback> _callback;
    std::string _buffer;  
    bool _is_complete = false;
};



class AIService
{
public:
	enum RequestMode :std::uint8_t
	{
		Sync = 0,
		Stream = 1
	};

	class CURLRequest
	{
		friend class AIService;
	public:
		CURLRequest();
		~CURLRequest();
		CURLRequest(const CURLRequest&) = delete;
		CURLRequest& operator=(const CURLRequest&) = delete;
		//allow move
		CURLRequest(CURLRequest&& other) noexcept;
		CURLRequest& operator=(CURLRequest&& other) noexcept;
		bool IsAllValid() const;
	private:
		CURL* _curl;
		curl_slist* _headers;
	};

public:
	//static std::optional<std::string> SendRequest(
	//	const Json::Value& data, const std::string& token, const std::string& url);

	static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

	static size_t StreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

	static bool SendStreamRequest(const Json::Value& data,
	                              const std::string& token,
	                              const std::string& url,
	                              std::shared_ptr<StreamCallback> callback = nullptr);

	static bool SendRequest(const Json::Value& data,
	                        const std::string& token,
	                        const std::string& url, std::string& resp);

	static bool SendStreamReq(const Json::Value& data,
		const std::string& token,
		const std::string& url, std::string& resp, std::shared_ptr<StreamCallback> callback);

	static bool SendSyncReq(std::shared_ptr<CURLRequest> curl_req, std::string& response);

private:
    static void ProcessStreamBuf(StreamContext* context);
};

