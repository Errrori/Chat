#include "pch.h"
#include "MessageService.h"
#include "models/Messages.h"
#include "Common/ChatMessage.h"
#include "ConnectionService.h"
#include "ThreadService.h"
#include "Common/AIMessage.h"
#include "Common/AIRequestMsg.h"


drogon::Task<Json::Value> MessageService::GetChatRecords(int thread_id, int num, int64_t existed_id)
{
	try
	{
		if (thread_id <= 0 || num <= 0||existed_id<0)
		{
			LOG_ERROR << "invalid parameters";
			co_return Json::nullValue;
		}
		
		co_return co_await _msg_repo->GetMessageRecords(thread_id,existed_id,num);
	}catch (const std::exception& e)
	{
		throw;
	}

}

drogon::Task<Json::Value> MessageService::GetAIRecords(int thread_id, int64_t existed_time)
{
	try
	{
		if (thread_id <= 0 || existed_time < 0)
		{
			LOG_ERROR << "invalid parameters";
			co_return Json::nullValue;
		}
		//check time if its existed in records and valid
		co_return co_await _msg_repo->GetAIContext(thread_id,existed_time);
	}catch (const std::exception& e)
	{
		throw;
	}
}

void MessageService::ProcessUserMsg(ChatMessage msg, const ErrorCb& cb) const
{
	//message here contains attachment/content/thread_id/sender_info

	try
	{
		drogon::async_run([this,msg = std::move(msg),cb]() mutable ->drogon::Task<>
		{
			try
			{
				bool result = co_await _thread_service->ValidateMember(msg.getThreadId(),
					msg.getSenderUid());

				if (!result)
				{
					LOG_ERROR << "user is not in the thread!";
					cb(Utils::GenErrorResponse("user is not in thread", ChatCode::NotPermission));
					co_return;
				}
				auto msg_id = co_await _msg_repo->RecordUserMessage(msg);
				msg.setMessageId(msg_id);

				if (!msg.IsValid())
				{
					LOG_ERROR << "message to delivered is not valid";
					cb("message to delivered is not valid");
					co_return;
				}
				auto members = co_await _thread_service->GetThreadMember(msg.getThreadId());

				Json::Value json_msg;
				json_msg["thread_id"] = msg.getThreadId();
				json_msg["msg_id"] = (Json::Value::Int64)msg.getMessageId();
				json_msg["content"] = msg.getContent();
				json_msg["attachment"] = msg.getAttachment();
				json_msg["sender_uid"] = msg.getSenderUid();
				json_msg["sender_name"] = msg.getSenderName();
				json_msg["sender_avatar"] = msg.getSenderAvatar();
				json_msg["status"] = msg.getStatus();
				json_msg["create_time"] = (Json::Value::Int64)msg.getCreateTime();
				json_msg["update_time"] = (Json::Value::Int64)msg.getUpdateTime();

				LOG_INFO << "send message: " << json_msg.toStyledString();

				_conn_service->Broadcast(members, json_msg);
			}catch (const std::exception& e)
			{
				LOG_ERROR << "exception: " << e.what();
				cb(Utils::GenErrorResponse("can not store the message", ChatCode::SystemException));
			}
		});
	}catch (const std::exception& e)
	{
		LOG_ERROR << "exception in ProcessMessage: " << e.what();
		cb(Utils::GenErrorResponse("can not store the message", ChatCode::SystemException));
	}

}



void MessageService::ProcessAIRequest(Json::Value msg, drogon::WebSocketConnectionPtr conn) const
{

	drogon::async_func([msg = std::move(msg),conn = std::move(conn),this]()->drogon::Task<>
	{
		try
		{
			AIMessage ai_msg = AIMessage::FromJson(msg);

			bool result = co_await _thread_service->ValidateMember(ai_msg.getThreadId(),
				conn->getContext<Utils::UsersInfo>()->uid);

			if (!result)
			{
				LOG_ERROR << "user is not in the thread!";
				conn->sendJson(Utils::GenErrorResponse("user is not in thread", ChatCode::NotPermission));
				co_return;
			}

			auto type = co_await _thread_service->GetThreadType(ai_msg.getThreadId());
			if (type != ChatThread::AI)
			{
				LOG_ERROR << "thread id is not corresponding to an AI thread";
				conn->sendJson(Utils::GenErrorResponse("unmatched type", ChatCode::UnMatchedType));
				co_return;
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
				co_return;//handle
			}

			//get context before record new message
			auto json_context = co_await _msg_repo->GetAIContext(ai_msg.getThreadId());
			//push message to database
			co_await _msg_repo->RecordAIMessage(ai_msg);

			RequestMsg req_msg(msg["content"].asString(), Utils::Authentication::GenerateUid(), RequestMsg::Role::User);
			//build request msg , if has context ,append
			//here need to check request data fields
			//.....

			const auto& req_data = msg["request_data"];
			if (req_data.isMember("is_stream") && req_data["is_stream"].isBool())
				req_msg.SetStream(req_data["is_stream"].asBool());
			req_msg.SetContext(json_context);

			conn->sendJson(req_msg.ToJsonReq());

			auto response = co_await AIRequestProcessor()
				("https://open.bigmodel.cn/api/paas/v4/chat/completions", "45664786303e46ca9caa8dc3d82ce7c4.VzuV8oSl4Nb2H3GU"
					, ai_msg.getThreadId(), req_msg, [conn](const Json::Value& resp)
					{
						conn->sendJson(resp);
					});

			co_await _msg_repo->RecordAIMessage(response);
		}
		catch (const std::exception& e)
		{
			LOG_ERROR << "exception: " << e.what();
			conn->sendJson(Utils::GenErrorResponse("server exception: "+std::string(e.what()),ChatCode::SystemException));
		}
	}
	);
}


MessageService::AIRequestProcessor::AIRequestProcessor()
{
	_curl = curl_easy_init();
}

drogon::Task<AIMessage> MessageService::AIRequestProcessor::operator()(const std::string& url, const std::string& token,int thread_id,
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
			throw std::invalid_argument("request data is not valid");
		}
		const auto& req_body = Json::writeString(builder, req.ToJsonReq());

		LOG_INFO << "send request: " << req_body;

		curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, req_body.c_str());
		curl_easy_setopt(_curl, CURLOPT_POSTFIELDSIZE, req_body.length());

		_headers = curl_slist_append(_headers, ("Authorization: " + token).c_str());
		_headers = curl_slist_append(_headers, "Content-Type: application/json");
		curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, _headers);

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
				throw std::runtime_error("request perform fail");
			}

			AIMessage response(thread_id,req.GetRequestId(),content._acc_content,
				Utils::GetCurrentTimeStamp(),AIMessage::Role::assistant);
			response.setReasoningContent(content._acc_reason);

			co_return response;
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
			throw std::runtime_error("request perform fail");
		}
		//lack of process Json
		Json::Value json_resp;
		Json::Reader reader;
		if (!reader.parse(resp, json_resp))
		{
			send_cb(Utils::GenErrorResponse("can not parse response", ChatCode::InValidJson,req_id));
			throw std::invalid_argument("can not parse response");
		}

		if (json_resp.isMember("error"))
		{
			send_cb(Utils::GenErrorResponse(json_resp["error"]["message"].toStyledString(), ChatCode::BadAIRequest, req_id));
			throw std::runtime_error("request error: "+ json_resp["error"]["message"].asString());
		}

		auto resp_content = json_resp["choices"][0]["message"];
		resp_content["thread_id"] = thread_id;
		resp_content["message_id"] = req.GetRequestId();
		LOG_INFO << "response:" << resp_content.toStyledString();

		send_cb(resp_content);

		AIMessage response(thread_id, req.GetRequestId(), resp_content["content"].asString(),
			Utils::GetCurrentTimeStamp(), AIMessage::Role::assistant);

		response.setReasoningContent(resp_content["reasoning_content"].asString());

		co_return response;

	}catch (const std::exception& e)
	{
		LOG_ERROR << "exception in AIRequestProcessor: " << e.what();
		throw;
	}
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

		if (line.substr(0, 6) == "data: ")
		{
			std::string data = line.substr(6);
			if (data == terminator)
			{
				context->_is_complete = true;
				Json::Value final_msg;
				final_msg["is_complete"] = true;
				final_msg["thread_id"] = context->_thread_id;
				final_msg["message_id"] = context->_req_id;
				final_msg["complete_content"] = context->_acc_content;
				final_msg["complete_reason"] = context->_acc_reason;
				context->_cb(final_msg);
				return total_size;
			}

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
