#include "pch.h"
#include "Common/ResponseHelper.h"
#include "MessageService.h"
#include "models/Messages.h"
#include "Common/ChatMessage.h"
#include "ConnectionService.h"
#include "Container.h"
#include "ThreadService.h"
#include "Common/AIMessage.h"
#include "Common/AIRequestMsg.h"
#include "RelationshipService.h"
#include "RedisService.h"
#include "UserService.h"


drogon::Task<Json::Value> MessageService::GetChatRecords(int thread_id, int num, int64_t existed_id)
{
	try
	{
		if (thread_id <= 0 || num <= 0 || existed_id < 0)
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
				// MSVC coroutine state machine requires all variables to be declared
				// before the first co_await, otherwise they appear as "undeclared".
				const int thread_id = msg.getThreadId();
				const std::string sender_uid = msg.getSenderUid();
				int64_t msg_id = 0;
				std::string block_target;

				// Step 1: 1次联合查询获取 thread_type + members（2次 DB，替换原来的 9次）
				auto thread_info = co_await _thread_service->GetTypeAndMembers(thread_id);
				const ChatThread::ThreadType thread_type = thread_info.first;
				std::vector<std::string> members = std::move(thread_info.second);

				if (members.empty())
				{
					LOG_ERROR << "thread has no members, thread_id: " << thread_id;
					cb(ResponseHelper::MakeErrorJson("invalid thread", ChatCode::NotPermission));
					co_return;
				}

				// Step 2: 成员校验（内存操作，0次 DB）
			LOG_DEBUG << "Thread " << thread_id << " members check - sender_uid: [" << sender_uid << "], members count: " << members.size();
			for (size_t i = 0; i < members.size(); ++i) {
				LOG_DEBUG << "  member[" << i << "]: [" << members[i] << "]";
			}
			if (std::find(members.begin(), members.end(), sender_uid) == members.end())
			{
				LOG_ERROR << "user not in thread, tid=" << thread_id << " uid=" << sender_uid;
				cb(ResponseHelper::MakeErrorJson("user is not in thread", ChatCode::NotPermission));
				co_return;
			}
			LOG_DEBUG << "Member validation passed for uid: " << sender_uid;
				// Step 3: block check for private threads (1 DB op)
				if (thread_type == ChatThread::PRIVATE)
				{
					for (const auto& m : members)
					{
						if (m != sender_uid) { block_target = m; break; }
					}
				}
				if (!block_target.empty())
				{
					bool blocked = co_await _relationship_service->IsBlocked(sender_uid, block_target);
					if (blocked)
					{
						cb(ResponseHelper::MakeErrorJson("Cannot send message due to block relationship", ChatCode::NotPermission));
						co_return;
					}
				}

				// Step 4: store message (1 DB write)
				msg_id = co_await _msg_repo->RecordUserMessage(msg);
				msg.setMessageId(msg_id);

				if (!msg.IsValid())
				{
					LOG_ERROR << "message to delivered is not valid";
					cb(ResponseHelper::MakeErrorJson("message data is not valid", ChatCode::InvalidArg));
					co_return;
				}

				const auto json_msg = msg.ToMessage().value();
				LOG_INFO << "send message: " << json_msg.toStyledString();

				// Step 5: unified delivery (online send + optional offline queue)
				for (const auto& target_uid : members)
				{
					auto outbound = ChatDelivery::OutboundMessage::Chat(
						msg,
						target_uid == sender_uid
							? ChatDelivery::DeliveryPolicy::OnlineOnly
							: ChatDelivery::DeliveryPolicy::PreferOnline);

					auto result = co_await _conn_service->DeliverToUser(target_uid, outbound);
					if (result.state == ChatDelivery::DeliveryState::Queued)
						LOG_INFO << "Queued offline message for: " << target_uid;
				}
			}catch (const std::exception& e)
			{
				LOG_ERROR << "exception: " << e.what();
				cb(ResponseHelper::MakeErrorJson("can not store the message", ChatCode::SystemException));
			}
		});
	}catch (const std::exception& e)
	{
		LOG_ERROR << "exception in ProcessMessage: " << e.what();
		cb(ResponseHelper::MakeErrorJson("can not store the message", ChatCode::SystemException));
	}

}

drogon::Task<MsgProcessedResult> MessageService::ProcessChatMsg(int thread_id, const std::string& sender_uid,
                                                                std::optional<std::string> content,
                                                                std::optional<Json::Value> attachment) const
{
	auto user_info = co_await Container::GetInstance().GetUserService()->GetDisplayProfileByUid(sender_uid);
	if (!user_info.IsValid())
		throw std::invalid_argument("can not get sender info");

	auto thread_info = co_await _thread_service->GetTypeAndMembers(thread_id);
	const ChatThread::ThreadType thread_type = thread_info.first;
	std::vector<std::string> members = std::move(thread_info.second);
	if (members.empty())
		throw std::invalid_argument("thread has no members,thread id: " + std::to_string(thread_id));

	if (std::find(members.begin(), members.end(), 
		sender_uid) == members.end())
		throw std::invalid_argument("user is not in thread");

	
	if (thread_type == ChatThread::PRIVATE)
	{
		std::string block_target = (members[0] == sender_uid) ? members[1] : members[0];

		if (!block_target.empty())
		{
			bool blocked = co_await _relationship_service->IsBlocked(sender_uid, block_target);
			if (blocked)
				co_return { .success = false, .error = "can not send message due to be blocked" };
		}
	}

	ChatMessage message;
	message.setThreadId(thread_id);
	if (attachment)
		message.setAttachment(*attachment);
	if (content)
		message.setContent(*content);
	message.setSenderUid(sender_uid);
	message.setSenderAvatar(*user_info.GetAvatar());
	message.setSenderName(*user_info.GetUsername());

	auto msg_id = co_await _msg_repo->RecordUserMessage(message);
	message.setMessageId(msg_id);

	if (!message.IsValid())
	{
		LOG_ERROR << "message to delivered is not valid";
		throw std::invalid_argument("message is not valid");
	}

	// Step 5: unified delivery (online send + optional offline queue)
	for (const auto& target_uid : members)
	{
		auto outbound = ChatDelivery::OutboundMessage::Chat(
			message,
			target_uid == sender_uid
			? ChatDelivery::DeliveryPolicy::OnlineOnly
			: ChatDelivery::DeliveryPolicy::PreferOnline);

		auto result = co_await _conn_service->DeliverToUser(target_uid, outbound);
	}

	co_return{ .success = true };
}


void MessageService::ProcessAIRequest(Json::Value msg, drogon::WebSocketConnectionPtr conn) const
{

	drogon::async_run([msg = std::move(msg),conn = std::move(conn),this]()->drogon::Task<>
	{
		try
		{
			AIMessage ai_msg = AIMessage::FromJson(msg);

			auto type = co_await _thread_service->GetThreadType(ai_msg.getThreadId());
			if (type != ChatThread::AI)
			{
				LOG_ERROR << "thread id is not corresponding to an AI thread";
                Utils::SendJson(conn, ResponseHelper::MakeErrorJson("unmatched type", ChatCode::UnMatchedType));
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
                Utils::SendJson(conn, ResponseHelper::MakeErrorJson("invalid AI message", ChatCode::MissingField));
				co_return;//handle
			}

			//get context before record new message
			auto json_context = co_await _msg_repo->GetAIContext(ai_msg.getThreadId());
			//push message to database
			co_await _msg_repo->RecordAIMessage(ai_msg);

			RequestMsg req_msg(Utils::Authentication::GenerateUid(),msg["content"].asString(),RequestMsg::Role::User);
			//build request msg , if has context ,append
			//here need to check request data fields
			//.....

			const auto& req_data = msg["request_data"];
			if (req_data.isMember("is_stream") && req_data["is_stream"].isBool())
				req_msg.SetStream(req_data["is_stream"].asBool());
			if (req_data.isMember("is_thinking")&&req_data["is_thinking"].isBool())
				req_msg.SetThinking(req_data["is_thinking"].asBool());
			
			req_msg.SetContext(json_context);

            Utils::SendJson(conn, req_msg.ToJsonReq());


			auto response = co_await AIRequestProcessor()
				("https://open.bigmodel.cn/api/paas/v4/chat/completions", "45664786303e46ca9caa8dc3d82ce7c4.VzuV8oSl4Nb2H3GU"
					, ai_msg.getThreadId(), req_msg, [conn](const Json::Value& resp)
					{
                        Utils::SendJson(conn, resp);
					});
			co_await _msg_repo->RecordAIMessage(response);
		}
		catch (const std::exception& e)
		{
			LOG_ERROR << "exception: " << e.what();
            Utils::SendJson(conn, ResponseHelper::MakeErrorJson("server exception: "+std::string(e.what()),ChatCode::SystemException));
		}
	}
	);
}

void MessageService::ProcessRequest(const std::string& url, const std::string& token, int thread_id,
                                    const RequestMsg& req_data, drogon::WebSocketConnectionPtr conn) const
{
	try
	{
		// ������ URL ��ȡ����ַ��·��
		const auto scheme_pos = url.find("://");
		const auto host_start = (scheme_pos == std::string::npos) ? 0 : scheme_pos + 3;
		const auto path_pos = url.find('/', host_start);
		const std::string base = (path_pos == std::string::npos) ? url : url.substr(0, path_pos);
		const std::string path = (path_pos == std::string::npos) ? "/" : url.substr(path_pos);

		// ����ͻ��˽�ʹ�û�ַ���� https://open.bigmodel.cn��
		auto client = drogon::HttpClient::newHttpClient(base);

		// ���� POST JSON ��������·����ͷ��
		auto req = drogon::HttpRequest::newHttpJsonRequest(req_data.ToJsonReq());
		req->setMethod(drogon::Post);
		req->setPath(path); // �� /api/paas/v4/chat/completions
		req->addHeader("Content-Type", "application/json");
		req->addHeader("Authorization", "Bearer " + token);
		req->addHeader("Accept", "application/json"); // ��ѡ

		auto is_stream = req_data.IsStream();

        client->sendRequest(req, [is_stream,conn](drogon::ReqResult ret, const drogon::HttpResponsePtr& resp)
            {
                conn->send(std::string(resp->getBody()), drogon::WebSocketMessageType::Text);
            });


	}catch (const std::exception& e)
	{
		throw;
	}
}

drogon::Task<Json::Value> MessageService::GetChatOverviews(int64_t existing_id, const std::string& uid) const
{
	try
	{
		co_return co_await _msg_repo->GetChatOverviews(existing_id, uid);
	}
	catch (const std::exception& e)
	{
		throw;
	}
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
				send_cb(ResponseHelper::MakeErrorJson("request perform fail ", ChatCode::BadAIRequest,req.GetRequestId()));
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
			send_cb(ResponseHelper::MakeErrorJson("request perform fail ", ChatCode::BadAIRequest, req_id));
			throw std::runtime_error("request perform fail");
		}
		//lack of process Json
		Json::Value json_resp;
		Json::Reader reader;
		if (!reader.parse(resp, json_resp))
		{
			send_cb(ResponseHelper::MakeErrorJson("can not parse response", ChatCode::InValidJson,req_id));
			throw std::invalid_argument("can not parse response");
		}

		if (json_resp.isMember("error"))
		{
			send_cb(ResponseHelper::MakeErrorJson(json_resp["error"]["message"].toStyledString(), ChatCode::BadAIRequest, req_id));
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
						context->_cb(ResponseHelper::MakeErrorJson(chunk_data["error"]["message"].asString(),ChatCode::BadAIRequest));
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
					context->_cb(ResponseHelper::MakeErrorJson("can not parse response",ChatCode::InValidJson));
				}
			}
			catch (std::exception& e) {
				LOG_ERROR << "exception in parsing stream chunk: " << e.what();
				context->_cb(ResponseHelper::MakeErrorJson(e.what(), ChatCode::SystemException,context->_req_id));
			}
		}
	}
	return total_size;
}
