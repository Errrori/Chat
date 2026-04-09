#include "pch.h"
#include <drogon/HttpClient.h>
#include <sstream>
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

namespace
{
struct DeliverySummary
{
	int sent_count{0};
	int queued_count{0};
	int dropped_count{0};
	int redis_failed_queue_count{0};
	std::vector<std::string> failed_uids;

	void Accumulate(const std::string& uid, const ChatDelivery::DeliveryResult& result)
	{
		switch (result.state)
		{
		case ChatDelivery::DeliveryState::Sent:
			++sent_count;
			break;
		case ChatDelivery::DeliveryState::Queued:
			++queued_count;
			break;
		case ChatDelivery::DeliveryState::Dropped:
			++dropped_count;
			break;
		}

		if (result.IsRedisQueueFailed())
		{
			++redis_failed_queue_count;
			if (failed_uids.size() < 8)
				failed_uids.push_back(uid);
		}
	}
};
}


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
				DeliverySummary summary;

				// Step 5: unified delivery (online send + optional offline queue)
				for (const auto& target_uid : members)
				{
					auto outbound = ChatDelivery::OutboundMessage::Chat(
						msg,
						target_uid == sender_uid
							? ChatDelivery::DeliveryPolicy::OnlineOnly
							: ChatDelivery::DeliveryPolicy::PreferOnline);

					auto result = co_await _conn_service->DeliverToUser(target_uid, outbound);
					summary.Accumulate(target_uid, result);
					if (result.state == ChatDelivery::DeliveryState::Queued)
						LOG_INFO << "Queued offline message for: " << target_uid;
					if (result.IsRedisQueueFailed())
					{
						LOG_ERROR << "[MessageDelivery] Redis queue degraded in ProcessUserMsg, thread_id="
							<< thread_id << ", message_id=" << msg_id << ", target_uid=" << target_uid;
					}
				}

				if (summary.redis_failed_queue_count > 0)
				{
					LOG_ERROR << "[MessageDelivery] ProcessUserMsg completed with degraded offline queueing, thread_id="
						<< thread_id << ", message_id=" << msg_id
						<< ", redis_failed_targets=" << summary.redis_failed_queue_count;
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
	DeliverySummary summary;

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
		summary.Accumulate(target_uid, result);
		if (result.IsRedisQueueFailed())
		{
			LOG_ERROR << "[MessageDelivery] Redis queue degraded in ProcessChatMsg, thread_id="
				<< thread_id << ", message_id=" << msg_id << ", target_uid=" << target_uid;
		}
	}

	co_return{
		.success = true,
		.error = "",
		.partial_degraded = summary.redis_failed_queue_count > 0,
		.redis_failed_targets = summary.redis_failed_queue_count
	};
}


// ── 文件作用域 AI 响应解析辅助函数 ─────────────────────────────────────────

namespace
{
constexpr auto kAiHost = "https://open.bigmodel.cn";
constexpr auto kAiPath = "/api/paas/v4/chat/completions";

using AISendCb = std::function<void(const Json::Value&)>;

/// 解析 AI 同步（非流式）JSON 响应，填充 out_msg 并推送结果给客户端
void ParseNonStreamResponse(const std::string& body,
    const std::string& req_id, int thread_id,
    const AISendCb& send_cb, AIMessage& out_msg)
{
    Json::Value json_resp;
    Json::Reader reader;
    if (!reader.parse(body, json_resp))
    {
        send_cb(ResponseHelper::MakeErrorJson("can not parse AI response", ChatCode::InValidJson, req_id));
        throw std::invalid_argument("can not parse AI response");
    }

    if (json_resp.isMember("error"))
    {
        const auto err = json_resp["error"]["message"].asString();
        send_cb(ResponseHelper::MakeErrorJson(err, ChatCode::BadAIRequest, req_id));
        throw std::runtime_error("AI API error: " + err);
    }

    auto resp_content = json_resp["choices"][0]["message"];
    resp_content["thread_id"]  = thread_id;
    resp_content["message_id"] = req_id;
    LOG_INFO << "AI response: " << resp_content.toStyledString();

    send_cb(resp_content);
    out_msg.setContent(resp_content["content"].asString());
    out_msg.setReasoningContent(resp_content["reasoning_content"].asString());
}

/// 解析 SSE 流式响应 body（HttpClient 一次性返回），逐 chunk 推送给客户端
void ParseSseBodyResponse(const std::string& body,
    const std::string& req_id, int thread_id,
    const AISendCb& send_cb, AIMessage& out_msg)
{
    static const std::string kDone = "[DONE]";
    std::istringstream stream(body);
    std::string line;
    std::string acc_content;
    std::string acc_reason;

    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.size() < 6 || line.substr(0, 6) != "data: ")
            continue;

        const std::string data = line.substr(6);

        if (data == kDone)
        {
            Json::Value final_msg;
            final_msg["is_complete"]      = true;
            final_msg["thread_id"]        = thread_id;
            final_msg["message_id"]       = req_id;
            final_msg["complete_content"] = acc_content;
            final_msg["complete_reason"]  = acc_reason;
            send_cb(final_msg);
            break;
        }

        Json::Value chunk;
        Json::Reader reader;
        if (!reader.parse(data, chunk))
        {
            send_cb(ResponseHelper::MakeErrorJson("can not parse SSE chunk", ChatCode::InValidJson, req_id));
            continue;
        }

        if (chunk.isMember("error"))
        {
            send_cb(ResponseHelper::MakeErrorJson(
                chunk["error"]["message"].asString(), ChatCode::BadAIRequest, req_id));
            break;
        }

        if (chunk.isMember("choices") && chunk["choices"].isArray() && !chunk["choices"].empty())
        {
            const auto& delta = chunk["choices"][0]["delta"];
            Json::Value resp;
            resp["message"]    = delta;
            resp["request_id"] = req_id;
            resp["thread_id"]  = thread_id;
            send_cb(resp);

            if (delta.isMember("content"))
                acc_content += delta["content"].asString();
            if (delta.isMember("reasoning_content"))
                acc_reason  += delta["reasoning_content"].asString();
        }
    }

    out_msg.setContent(acc_content);
    out_msg.setReasoningContent(acc_reason);
}
} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────

void MessageService::ProcessAIRequest(Json::Value msg, drogon::WebSocketConnectionPtr conn) const
{
    drogon::async_run([msg = std::move(msg), conn = std::move(conn), this]() -> drogon::Task<>
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
            ai_msg.setRole(AIMessage::Role::user);
            ai_msg.setReasoningContent({});

            if (!ai_msg.IsValid())
            {
                LOG_ERROR << "AI message is not valid";
                Utils::SendJson(conn, ResponseHelper::MakeErrorJson("invalid AI message", ChatCode::MissingField));
                co_return;
            }

            // 记录用户消息前先获取历史上下文
            auto json_context = co_await _msg_repo->GetAIContext(ai_msg.getThreadId());
            co_await _msg_repo->RecordAIMessage(ai_msg);

            RequestMsg req_msg(Utils::Authentication::GenerateUid(),
                               msg["content"].asString(), RequestMsg::Role::User);

            const auto& req_data = msg["request_data"];
            if (req_data.isMember("is_stream") && req_data["is_stream"].isBool())
                req_msg.SetStream(req_data["is_stream"].asBool());
            if (req_data.isMember("is_thinking") && req_data["is_thinking"].isBool())
                req_msg.SetThinking(req_data["is_thinking"].asBool());

            req_msg.SetContext(json_context);

            // 向客户端回显请求体（预览）
            Utils::SendJson(conn, req_msg.ToJsonReq());

            // ── Phase 4/5: drogon::HttpClient 替换 cURL ──────────────────
            auto client   = drogon::HttpClient::newHttpClient(kAiHost);
            auto http_req = drogon::HttpRequest::newHttpJsonRequest(req_msg.ToJsonReq());
            http_req->setMethod(drogon::Post);
            http_req->setPath(kAiPath);
            http_req->addHeader("Authorization", API_KEY::key1);

            auto resp    = co_await client->sendRequestCoro(http_req, 60.0);
            const auto body    = std::string(resp->getBody());
            const auto req_id  = req_msg.GetRequestId();
            const int  tid     = ai_msg.getThreadId();

            AIMessage response_msg(tid, req_id, "", Utils::GetCurrentTimeStamp(), AIMessage::Role::assistant);
            const auto send_cb = [&conn](const Json::Value& v) { Utils::SendJson(conn, v); };

            if (req_msg.IsStream())
                ParseSseBodyResponse(body, req_id, tid, send_cb, response_msg);
            else
                ParseNonStreamResponse(body, req_id, tid, send_cb, response_msg);

            co_await _msg_repo->RecordAIMessage(response_msg);
        }
        catch (const drogon::HttpException& e)
        {
            LOG_ERROR << "HttpClient exception in ProcessAIRequest: " << e.what();
            Utils::SendJson(conn, ResponseHelper::MakeErrorJson(
                std::string("AI service error: ") + e.what(), ChatCode::BadAIRequest));
        }
        catch (const std::exception& e)
        {
            LOG_ERROR << "exception in ProcessAIRequest: " << e.what();
            Utils::SendJson(conn, ResponseHelper::MakeErrorJson(
                "server exception: " + std::string(e.what()), ChatCode::SystemException));
        }
    });
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
