#pragma once
#include <curl/curl.h>


namespace API_KEY
{
	const std::string key1 = "Bearer 45664786303e46ca9caa8dc3d82ce7c4.VzuV8oSl4Nb2H3GU";
}

namespace SERVICE_URL
{
	const std::string CHAT_URL = "https://open.bigmodel.cn/api/paas/v4/chat/completions";
}

static const std::string terminator = "[DONE]";

//Request Data
//{
//	  用户填充字段：
//	  thread_id
//	  content
//	  request_data:{
//		max_tokens (optional,default 1024)
//		is_stream (optional,default false)
//		do_sample (optional,default true)
//		model (optional,default glm-4.5-flash)
//	  }
//
//    服务器填充字段:
//	  role
//	  context
//    request_id
//}

//DataBase Data
//{
//    用户填充字段：
//    thread_id
//    content
//    attachment(optional)
//
//    服务端填充字段
//    message_id
//    role
//    reasoning_content(ai only)
//    created_time
//}

//Json
//{
//    attachment(optional) 暂时闲置,
//    thread_id,
//	  request_id (server),
//    role (server),
//	  content,
//    reasoning_content,
//    request_data:{
//		is_stream (default false)
//		max_tokens (optional,default 1024)
//		do_sample (optional,default true)
//		model (optional,default glm-4.5-flash)
//    }
//}

//response(stream)
//{
//    request_id:" ",
//	  messages : {
//	  "role":"assistant",
//	  "content":"解决问题"
//	  }
//}
//or
//{
//    request_id:" ",
//	  messages : {
//	  "role":"assistant",
//	  "reasoning_content":"解决问题"
//	  }
//}

//response(not stream)
//{
//    request_id:" ",
//	  messages : {
//	  "role":"assistant",
//	  "reasoning_content":"原因",
//	  "content":"解决问题"
//	  }
//}


struct StreamContent
{
	StreamContent(const std::function<void(const Json::Value&)>& cb,int thread_id,const std::string& request_id)
		:_cb(cb), _thread_id(thread_id),_req_id(request_id)
	{}

	std::string _buffer;
	std::function<void(const Json::Value&)> _cb;
	int _thread_id;
	std::string _req_id;
	bool _is_complete = false;
	std::string _acc_content;
	std::string _acc_reason;
};

class RequestMsg
{
public:
	enum Role :std::uint8_t
	{
		Unknown = 0,
		User,
		Assistant,
		Tool
	};

	enum Model :std::uint8_t
	{
		glm_flash,
		glm
	};

	RequestMsg(const std::string& request_id,const std::string& content,Role role = Role::Unknown,
		Model model = Model::glm_flash , bool is_stream = false, bool is_thinking = true , 
		bool do_sample = true,int max_token = 1024) :
		_request_id(request_id),_content(content), _role(role),
		_model(model),_is_stream(is_stream),_do_sample(do_sample),
		_is_thinking(is_thinking), _max_tokens(max_token) {}


	static std::string RoleToString(Role role);
	static std::string ModelToString(Model model);

	Json::Value ToJsonReq() const;
	void SetContent(const std::string& content) { _content = content; }
	std::string GetContent() const { return _content; }

	void SetRole(Role role) { _role = role; }
	Role GetRole() const { return _role; }

	void SetStream(bool stream) { _is_stream = stream; }
	bool IsStream() const { return _is_stream; }

	void SetContext(const Json::Value& context);
	std::optional<Json::Value> GetContext() const { return _context; };

	void SetRequestId(const std::string& id) { _request_id = id; }
	std::string GetRequestId() const { return _request_id; }

	void SetThinking(bool flag) { _is_thinking = flag; }
	bool GetThinking() const { return _is_thinking; }

	bool IsValid() const;

private:
	std::string _url;
	std::string _token;
	std::string _request_id; 
	std::string _content;
	std::optional<Json::Value> _context;
	Role _role;
	Model _model = Model::glm_flash;
	bool _is_stream = false;
	bool _do_sample = true;
	bool _is_thinking = true;
	int _max_tokens = 1024;


	//model name/temperature/top_p
};

