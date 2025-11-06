#pragma once
#include <curl/curl.h>

#include "Common/AIMessage.h"
#include "Data/IMessageRepository.h"


namespace drogon_model::sqlite3
{
	class Messages;
}
class ChatThread;
class AIMessage;
class ConnectionService;
class ThreadService;
class drogon_model::sqlite3::Messages;
class IMsgProcessor;
class RequestMsg;

class MessageService
{
public:
	typedef std::function<void(const Json::Value&)> ErrorCb;

	MessageService(std::shared_ptr<IMessageRepository> repo, std::shared_ptr<ConnectionService> conn,std::shared_ptr<ThreadService> thread_service)
		:_msg_repo(std::move(repo)), _conn_service(std::move(conn)),_thread_service(std::move(thread_service)){}
	//,_thread_service(std::move(thread_service)))

	std::optional<int> RecordMessage(const ChatMessage& message) const;
	std::optional<int> RecordAIMessage(const ChatMessage& message) const;
	void DeliverMessage(const ChatMessage& message, const std::vector<drogon::WebSocketConnectionPtr>& targets,const ErrorCb& cb) const;

	Json::Value GetChatRecords(int thread_id,int64_t existed_id = 0);

	void ProcessMessage(ChatMessage msg, const ErrorCb& cb) const;
	void ProcessAIRequest(Json::Value msg, drogon::WebSocketConnectionPtr conn) const;

private:
	class AIRequestProcessor
	{
	public:
		typedef std::function<void(const Json::Value&)> SendCallback;
		AIRequestProcessor();

		//这里要么返回bool，或者在回调函数里写错误然后发送
		std::optional<AIMessage> operator()(const std::string& url, const std::string& token,int thread_id,
			const RequestMsg& req, const SendCallback& send_cb);

		~AIRequestProcessor();

	private:
		static size_t SyncWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
		static size_t StreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

		CURL* _curl = nullptr;
		curl_slist* _headers = nullptr;
	};

private:
	std::shared_ptr<IMessageRepository> _msg_repo;
	std::shared_ptr<ConnectionService> _conn_service;
	std::shared_ptr<ThreadService> _thread_service;
};

