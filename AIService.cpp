#include "pch.h"
#include "AIService.h"
#include "manager/ThreadManager.h"
#include <sstream>
#include <iomanip>
//#include <curl/curl.h>

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


size_t AIService::StreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	auto* context = static_cast<StreamContext*>(userp);

	if (context->_is_complete)
		return size * nmemb;

	auto total_size = size * nmemb;
	std::string chunk(static_cast<char*>(contents), total_size);
	context->_buffer += chunk;

	ProcessStreamBuf(context);

	return total_size;
}

void AIService::ProcessStreamBuf(StreamContext* context)
{
	auto& buf = context->_buffer;
	size_t pos = 0;

	while ((pos = buf.find('\n')) != std::string::npos)
	{
		std::string line = buf.substr(0, pos);
		buf.erase(0, pos + 1);

		if (line.length() >= 6 && line.substr(0, 6) == "data: ")
		{
			std::string data = line.substr(6);

			LOG_INFO << "stream buffer data: " << data;

			if (data == terminator)
			{
				context->_is_complete = true;
				context->_callback->onComplete();
				return;
			}

			try
			{
				Json::Value chunk_data;
				Json::Reader reader;
				if (reader.parse(data, chunk_data))
				{
					if (chunk_data.isMember("choices") &&
						chunk_data["choices"].isArray() &&
						!chunk_data["choices"].empty()) 
					{
						const auto& delta = chunk_data["choices"][0]["delta"];
						//here AI's response can be reasoning_content or content
						//but I'm not sure if this two field can be existed at the same response
						std::string content;
						if (delta.isMember("reasoning_content"))
							content = delta["reasoning_content"].asString();
						else if (delta.isMember("content"))
							content = delta["content"].asString();
						context->_callback->onChunk(content);
					}
				}
			}
			catch (std::exception& e){
				LOG_ERROR << "exception in parsing stream chunk: " << e.what();
			}
		}
	}
}


std::string ChatStreamCallback::GetContent() const
{
	return _accumulated_content;
}

void ChatStreamCallback::onChunk(const std::string& chunk)
{
	_accumulated_content += chunk;

	Json::Value stream_msg;
	stream_msg["role"] = "assistant";
	stream_msg["thread_id"] = _thread_id;
	stream_msg["content"] = chunk;
	stream_msg["_is_complete"] = false;

	_conn->sendJson(stream_msg);
}

void ChatStreamCallback::onComplete()
{
	Json::Value complete_msg;
	complete_msg["thread_id"] = _thread_id;
	complete_msg["content"] = _accumulated_content;
	complete_msg["role"] = "assistant";
	complete_msg["_is_complete"] = true;

	_conn->sendJson(complete_msg);
}

void ChatStreamCallback::onError(const std::string& error)
{
	Json::Value error_msg;
	error_msg["thread_id"] = _thread_id;
	error_msg["error"] = error;

	_conn->sendJson(error_msg);
}

AIService::CURLRequest::CURLRequest()
	:_curl(nullptr),_headers(nullptr)
{
	_curl = curl_easy_init();
	if (!_curl) {
		throw std::runtime_error("Failed to initialize CURL");
	}
}

AIService::CURLRequest::~CURLRequest()
{
	if (_curl)
		curl_easy_cleanup(_curl);
	if (_headers)
		curl_slist_free_all(_headers);
}

AIService::CURLRequest::CURLRequest(CURLRequest&& other) noexcept
	:_curl(other._curl), _headers(other._headers)
{
	other._curl = nullptr;
	other._headers = nullptr;
}

AIService::CURLRequest& AIService::CURLRequest::operator=(CURLRequest&& other) noexcept
{
	if (this != &other) 
	{
		if (_curl)
			curl_easy_cleanup(_curl);
		if (_headers)
			curl_slist_free_all(_headers);
		_curl = other._curl;
		_headers = other._headers;
		other._curl = nullptr;
		other._headers = nullptr;
	}
	return *this;
}

bool AIService::CURLRequest::IsAllValid() const
{
	return _curl && _headers;
}

bool AIService::SendRequest(const Json::Value& data,
                            const std::string& token,
                            const std::string& url, std::string& resp)
{
	try
	{
		curl_global_init(CURL_GLOBAL_ALL);
		auto curl = curl_easy_init();

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POST, 1L);

		Json::StreamWriterBuilder builder;
		builder["indentation"] = "";
		
		const auto& req_body = Json::writeString(builder, data);
		
		// 添加十六进制调试信息
		//LOG_INFO << "Request body to be sent: " << req_body.c_str();
		//std::stringstream req_hex;
		//req_hex << "Request body hex: ";
		//for (unsigned char c : req_body) {
		//	req_hex << std::hex << std::setfill('0') << std::setw(2) << (int)c << " ";
		//}
		//LOG_INFO << req_hex.str();
		
		// 使用 COPYPOSTFIELDS 以确保 libcurl 复制请求体，避免悬空指针
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req_body.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req_body.length());

		curl_slist* headers = nullptr;
		headers = curl_slist_append(headers, ("Authorization: " + token).c_str());
		headers = curl_slist_append(headers, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

		auto res_code = curl_easy_perform(curl);

		LOG_INFO << "response : " << resp;

		if (res_code != CURLE_OK)
		{
			return false;
		}

		return true;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception in initializing curl: " << e.what();
		return false;
	}

}

//here use && because we want to make sure the ptr to be clear after use the function
bool AIService::SendStreamReq(const Json::Value& data,
	const std::string& token,
	const std::string& url, std::string& resp, std::shared_ptr<StreamCallback> callback)
{
	curl_global_init(CURL_GLOBAL_ALL);
	auto curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);

	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";

	const auto& req_body = Json::writeString(builder, data);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req_body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req_body.length());

	curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, ("Authorization: " + token).c_str());
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 1024L);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

	StreamContext context;
	context._callback = callback;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
	CURLcode res_code = curl_easy_perform(curl);

	if (res_code != CURLE_OK)
	{
		LOG_ERROR << "curl error: " << std::string(curl_easy_strerror(res_code));
		context._callback->onError("CURL error: " + std::string(curl_easy_strerror(res_code)));
		return false;
	}

	return true;
}

bool AIService::SendSyncReq(std::shared_ptr<CURLRequest> curl_req, std::string& response)
{
	auto curl{ std::move(curl_req) };

	if (!curl->IsAllValid())
	{
		return false;
	}

	response.clear();
	curl_easy_setopt(curl->_curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl->_curl, CURLOPT_WRITEDATA, &response);

	auto res_code = curl_easy_perform(curl->_curl);

	LOG_INFO << "response : " << response;

	if (res_code!=CURLE_OK)
	{
		return false;
	}

	return true;
}