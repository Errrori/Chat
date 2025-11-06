#include "pch.h"
#include "MessageService.h"
#include "models/Messages.h"
#include "Common/ChatMessage.h"
#include "ConnectionService.h"
#include "ThreadService.h"
#include "Common/AIMessage.h"
#include "Common/AIRequestMsg.h"


//synchronous interface
std::optional<int> MessageService::RecordMessage(const ChatMessage& message) const
{
	try
	{
		auto future = _msg_repo->RecordUserMessage(message);
		auto id = future.get();

		return id;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception in RecordMessage: " << e.what();
		return std::nullopt;
	}
	catch (const drogon::orm::DrogonDbException& e)
	{
		LOG_ERROR << "database exception: " << e.base().what();
		return std::nullopt;
	}
}

void MessageService::DeliverMessage(const ChatMessage& message, 
	const std::vector<drogon::WebSocketConnectionPtr>& targets,const ErrorCb& cb) const
{

	if (!message.IsValid())
	{
		LOG_ERROR << "message to delivered is not valid";
		cb("message to delivered is not valid");
		return;
	}

	Json::Value json_msg;
	json_msg["thread_id"] = message.getThreadId();
	json_msg["message_id"] = message.getMessageId();
	json_msg["content"] = message.getContent();
	json_msg["attachment"] = message.getAttachment();
	json_msg["sender_uid"] = message.getSenderUid();
	json_msg["sender_name"] = message.getSenderName();
	json_msg["sender_avatar"] = message.getSenderAvatar();
	json_msg["status"] = message.getStatus();
	json_msg["create_time"] = message.getCreateTime();
	json_msg["update_time"] = message.getUpdateTime();

	for (const auto& target:targets)
	{
		if (target)
			target->sendJson(json_msg);
		else
			LOG_INFO << "connection is not active";
	}
}

Json::Value MessageService::GetChatRecords(int thread_id, int64_t existed_id)
{
	return _msg_repo->GetAIContext(thread_id).get();
}

void MessageService::ProcessMessage(ChatMessage msg, const ErrorCb& cb) const
{
	//message here contains attachment/content/thread_id/sender_info

	try
	{
		auto id = _msg_repo->RecordUserMessage(msg);

		auto msg_id = id.get();

		LOG_INFO << "message id: " << msg_id;

		msg.setMessageId(msg_id);
		const auto& members = _thread_service->GetThreadMember(msg.getThreadId());
		const auto& connections = _conn_service->GetConnections(members);

		DeliverMessage(msg, connections, cb);


	}catch (const std::exception& e)
	{
		LOG_ERROR << "exception in ProcessMessage: " << e.what();
		cb(Utils::GenErrorResponse("can not store the message", ChatCode::SystemException));
	}

}



void MessageService::ProcessAIRequest(Json::Value msg, drogon::WebSocketConnectionPtr conn) const
{
	try
	{
		//after check content and thread_id
		//build message
		AIMessage ai_msg = AIMessage::FromJson(msg);

		auto type = _thread_service->GetThreadType(ai_msg.getThreadId()).get();
		if (type!=ChatThread::AI)
		{
			LOG_ERROR << "thread id is not corresponding to an AI thread";
			conn->sendJson(Utils::GenErrorResponse("unmatched type", ChatCode::UnMatchedType));
			return;
		}

		ai_msg.setMessageId(Utils::Authentication::GenerateUid());
		//role should be set by server
		ai_msg.setRole(AIMessage::Role::user);
		//request should not have reasoning content
		ai_msg.setReasoningContent({});

		if (!ai_msg.IsValid())
		{
			LOG_ERROR << "AI message is not valid";
			conn->sendJson(Utils::GenErrorResponse("invalid AI message", ChatCode::MissingField));
			return;//handle
		}

		//get context before record new message
		const auto& json_context =  _msg_repo->GetAIContext(ai_msg.getThreadId()).get();
		//push message to database
		auto result = _msg_repo->RecordAIMessage(ai_msg);

		if (!result.get())
		{
			conn->sendJson(Utils::GenErrorResponse("can not store the message",ChatCode::SystemException));
			return;
		}


		RequestMsg req_msg(msg["content"].asString(),Utils::Authentication::GenerateUid(),RequestMsg::Role::User);
		//build request msg , if has context ,append
		//here need to check request data fields
		//.....
		const auto& req_data = msg["request_data"];
		if (req_data.isMember("is_stream")&&req_data["is_stream"].isBool())
			req_msg.SetStream(req_data["is_stream"].asBool());
		req_msg.SetContext(json_context);

		conn->sendJson(req_msg.ToJsonReq());

		auto response = 
			AIRequestProcessor()("https://open.bigmodel.cn/api/paas/v4/chat/completions", "45664786303e46ca9caa8dc3d82ce7c4.VzuV8oSl4Nb2H3GU"
			, ai_msg.getThreadId(), req_msg,[conn](const Json::Value& resp)
			{
				conn->sendJson(resp);
			});

		if (!response.has_value())
			conn->sendJson(Utils::GenErrorResponse("AI response is empty", ChatCode::SystemException));

		if (!_msg_repo->RecordAIMessage(response.value()).get())
			conn->sendJson(Utils::GenErrorResponse("can not store the AI response", ChatCode::SystemException));
		LOG_INFO << "content: " << response.value().getContent();
		LOG_INFO << "reason: " << response.value().getReasoningContent();
		conn->sendJson(response.value().getContent());
		conn->sendJson(response.value().getReasoningContent());

	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "exception in ProcessAIRequest: " << e.what();
	}
}


MessageService::AIRequestProcessor::AIRequestProcessor()
{
	_curl = curl_easy_init();
}

std::optional<AIMessage> MessageService::AIRequestProcessor::operator()(const std::string& url, const std::string& token,int thread_id,
	const RequestMsg& req, const SendCallback& send_cb)
{
	try
	{
		curl_easy_setopt(_curl, CURLOPT_POST, 1L);
		curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());

		Json::StreamWriterBuilder builder;
		builder["indentation"] = "";
		if (req.ToJsonReq() == Json::nullValue)
		{
			LOG_ERROR << "request data is not valid";
			return std::nullopt;
		}
		const auto& req_body = Json::writeString(builder, req.ToJsonReq());

		LOG_INFO << "send request: " << req_body;

		curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, req_body.c_str());
		curl_easy_setopt(_curl, CURLOPT_POSTFIELDSIZE, req_body.length());

		_headers = curl_slist_append(_headers, ("Authorization: " + token).c_str());
		_headers = curl_slist_append(_headers, "Content-Type: application/json");
		curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, _headers);

		//��ʽ���
		if (req.IsStream())
		{
			curl_easy_setopt(_curl, CURLOPT_BUFFERSIZE, 1024L);
			curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 0L);

			StreamContent content(send_cb,thread_id,req.GetRequestId());
			curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, StreamWriteCallback);
			curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &content);
			CURLcode res_code = curl_easy_perform(_curl);

			if (res_code != CURLE_OK)
			{
				LOG_ERROR << "curl error: " << std::string(curl_easy_strerror(res_code));
				send_cb(Utils::GenErrorResponse("request perform fail ", ChatCode::BadAIRequest,req.GetRequestId()));
				return std::nullopt;
			}

			if (content._is_complete)
			{
				LOG_INFO << "response is completed: " << content._buffer;
				return std::nullopt;
			}

			AIMessage response(thread_id,req.GetRequestId(),content._acc_content,
				Utils::GetCurrentTimeStamp(),AIMessage::Role::assistant);
			response.setReasoningContent(content._acc_reason);

			return response;
		}

		std::string resp;
		const auto& req_id = req.GetRequestId();

		curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, SyncWriteCallback);
		curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &resp);
		CURLcode res_code = curl_easy_perform(_curl);
		if (res_code != CURLE_OK)
		{
			LOG_ERROR << "curl error: " << std::string(curl_easy_strerror(res_code));
			send_cb(Utils::GenErrorResponse("request perform fail ", ChatCode::BadAIRequest, req_id));
			return std::nullopt;
		}
		//lack of process Json
		Json::Value json_resp;
		Json::Reader reader;
		if (!reader.parse(resp, json_resp))
		{
			send_cb(Utils::GenErrorResponse("can not parse response", ChatCode::InValidJson,req_id));
			return std::nullopt;
		}


		if (json_resp.isMember("error"))
		{
			send_cb(Utils::GenErrorResponse(json_resp["error"]["message"].toStyledString(), ChatCode::BadAIRequest, req_id));
			return std::nullopt;
		}

		auto resp_content = json_resp["choices"][0]["message"];
		resp_content["thread_id"] = thread_id;
		resp_content["message_id"] = req.GetRequestId();
		LOG_INFO << "response:" << resp_content.toStyledString();

		send_cb(json_resp["choices"][0]["message"]);

		AIMessage response(thread_id, req.GetRequestId(), resp_content["content"].asString(),
			Utils::GetCurrentTimeStamp(), AIMessage::Role::assistant);

		response.setReasoningContent(resp_content["reasoning_content"].asString());

		return response;

	}catch (const std::exception& e)
	{
		LOG_ERROR << "exception in AIRequestProcessor: " << e.what();
		send_cb(Utils::GenErrorResponse(e.what(),ChatCode::SystemException,req.GetRequestId()));
	}
	return std::nullopt;
}


MessageService::AIRequestProcessor::~AIRequestProcessor()
{
	if (_curl) {
		curl_easy_cleanup(_curl);
		_curl = nullptr;
	}
	if (_headers)
	{
		curl_slist_free_all(_headers);
		_headers = nullptr;
	}
}

size_t MessageService::AIRequestProcessor::SyncWriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	(static_cast<std::string*>(userp))->append(static_cast<char*>(contents), size * nmemb);
	return size * nmemb;
}

size_t MessageService::AIRequestProcessor::StreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	auto* context = static_cast<StreamContent*>(userp);

	if (context->_is_complete)
	{
		//idk if this will be executed
		LOG_INFO << "response is completed";
		return size * nmemb;
	}

	auto total_size = size * nmemb;
	std::string chunk(static_cast<char*>(contents), total_size);
	context->_buffer += chunk;

	auto& buf = context->_buffer;
	size_t pos = 0;

	while ((pos = buf.find('\n')) != std::string::npos)
	{
		std::string line = buf.substr(0, pos);
		buf.erase(0, pos + 1);

		if (line.starts_with("data: "))
		{
			std::string data = line.substr(6);
			if (data == terminator)
				return total_size;

			try
			{
				Json::Value chunk_data;
				Json::Reader reader;
				if (reader.parse(data, chunk_data))
				{
					if (chunk_data.isMember("error"))
					{
						context->_cb(Utils::GenErrorResponse(chunk_data["error"]["message"].asString(),ChatCode::BadAIRequest));
					}
					else if (chunk_data.isMember("choices") &&
						chunk_data["choices"].isArray() &&
						!chunk_data["choices"].empty())
					{

						//add uid to verify the message
						Json::Value resp;
						resp["message"] = chunk_data["choices"][0]["delta"];
						resp["request_id"] = context->_req_id;
						resp["thread_id"] = context->_thread_id;

						if (chunk_data["choices"][0]["delta"].isMember("content"))
							context->_acc_content += chunk_data["choices"][0]["delta"]["content"].asString();
						if (chunk_data["choices"][0]["delta"].isMember("reasoning_content"))
							context->_acc_reason += chunk_data["choices"][0]["delta"]["reasoning_content"].asString();

						context->_cb(resp);
					}
				}
				else
				{
					context->_cb(Utils::GenErrorResponse("can not parse response",ChatCode::InValidJson));
				}
			}
			catch (std::exception& e) {
				LOG_ERROR << "exception in parsing stream chunk: " << e.what();
				context->_cb(Utils::GenErrorResponse(e.what(), ChatCode::SystemException,context->_req_id));
			}
		}
	}
	return total_size;
}
