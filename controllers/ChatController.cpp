#include "pch.h"
#include "ChatController.h"
#include "../manager/ConnectionManager.h"
#include "MsgDispatcher.h"
#include "manager/ThreadManager.h"
#include "models/AiChats.h"
#include "AIService.h"
#include <sstream>
#include <iomanip>

//Message types:
//Image,Text,ErrorMessage,Relationship

void ChatController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& msg,
                                            const drogon::WebSocketMessageType& type)
{
    if (msg.empty())
    {
	    return;
    }

	LOG_INFO << "new message:" << msg;

    try
    {
        Json::Value msg_data;
        Json::Reader reader;
        ChatMessage chat_msg;
    	//现在暂时只会检查消息是否是json格式
        if (!reader.parse(msg, msg_data))
        {
            LOG_ERROR << "fail to parse message:" << msg;
            conn->send("fail to parse message");
            return;
        }

        // 添加JSON解析后的内容调试
        if (msg_data.isMember("content")&&!msg_data["content"].asString().empty()) {
            chat_msg.content = msg_data["content"].asString();
        }else if(!msg_data.isMember("attachment"))
		{
            LOG_ERROR << "attachment and content are empty";
            return;
		}

        if (msg_data.isMember("attachment"))
        {
            chat_msg.attachment = msg_data["attachment"];
        }

        if (!msg_data.isMember("thread_id"))
        {
            conn->send("lack of essential field");
            return;
        }

        auto thread_id = msg_data["thread_id"].asInt();
        chat_msg.thread_id = thread_id;

        const auto& info = conn->getContext<Utils::UsersInfo>();
        if (!info)
        {
            conn->send("can not get user info from connection!");
            return;
        }

        //检查用户是否有权限发言
        if (!ThreadManager::ValidateMember(info->uid, thread_id))
        {
            LOG_ERROR << "can not send in the thread";
            conn->send("you are not in the chat: " + std::to_string(thread_id));
            return;
        }
        chat_msg.sender_uid = info->uid;
        chat_msg.sender_name = info->username;
        chat_msg.sender_avatar = info->avatar;

		auto thread_type_opt = ThreadManager::GetThreadType(thread_id);
        if (!thread_type_opt.has_value())
        {
			LOG_ERROR << "can not get thread type";
            return;
        }


        switch (ChatThreads::ToType(thread_type_opt.value()))
        {
		case ChatThreads::ThreadType::PRIVATE:
        case ChatThreads::ThreadType::GROUP:
        {
            const auto& message = MessageManager::BuildMessage(info->uid, info->username, info->avatar, msg_data);
            if (!message.has_value())
            {
                //构造消息失败,消息格式错误
                LOG_ERROR << "Failed to build message";
                return;
            }

            const auto& message_record = ThreadManager::PushChatRecords(message.value());

			if (!message_record.has_value())
            {
                LOG_ERROR << "can not build record";
                return;
            }

            if (!MsgDispatcher::DeliverMessage(info->uid, thread_id, message_record.value().toJson()))
            {
				LOG_ERROR << "can not deliver message to thread: " << thread_id;
            }
            break;
        }

		case ChatThreads::ThreadType::AI:
			{
            Json::Value current_msg;
            current_msg["role"] = "user";
            current_msg["content"] = msg_data["content"].asString();

            // 添加AI消息处理调试
            const std::string user_content = current_msg["content"].asString();
            LOG_INFO << "User content for AI: [" << user_content << "]";
            std::stringstream user_content_hex;
            user_content_hex << "User content hex: ";
            for (unsigned char c : user_content) {
                user_content_hex << std::hex << std::setfill('0') << std::setw(2) << (int)c << " ";
            }
            LOG_INFO << user_content_hex.str();

            Json::StreamWriterBuilder builder;
            builder["indentation"] = "";
			chat_msg.content= Json::writeString(builder, current_msg);//here the message is completed
            //暂不支持attachment
            const auto& user_message = BuildMessages(chat_msg);

            if (!user_message.has_value())
            {
                LOG_ERROR << "Failed to build message";
                return;
            }

            LOG_INFO << "user message: " << user_message.value().getValueOfContent();
            //build ai chat context
            Json::Value context = ThreadManager::GetAIChatContext(thread_id);
            context.append(current_msg);

            Json::Value json_body;
            json_body["messages"] = context;
            json_body["model"] = "glm-4.5-flash";
            json_body["max_tokens"] = 2048;
            bool is_stream = false;
            if (msg_data.isMember("stream"))
            {
                is_stream = msg_data["stream"].asBool();
            }

            LOG_INFO << "Sending request to AI service, stream mode: " << is_stream << ", request body: " << json_body.toStyledString();

            std::string response;

            if (!is_stream)
            {
                if (!AIService::SendRequest(json_body, API_KEY::key1, SERVICE_URL::CHAT_URL, response))
                {
                    LOG_ERROR << "send request failed";
                    return;
                }
            }
            else
            {
                json_body["stream"] = is_stream;
                LOG_INFO << "Building stream request for AI service";
                auto chat_callback = std::make_shared<ChatStreamCallback>(conn, thread_id);
                if (!AIService::SendStreamReq(json_body, API_KEY::key1, SERVICE_URL::CHAT_URL, response,chat_callback))
				{
                    LOG_ERROR << "send stream request failed!";
                    return;
                }

                response = chat_callback->GetContent();

                LOG_INFO << "Stream request completed, final accumulated response: [" << response << "]";
            }
			
           conn->sendJson(response);
           Json::Value ai_message;
           ai_message["content"] = Json::writeString(builder, response);
           ai_message["role"] = "assistant";
           const auto& thread_info_opt = ThreadManager::GetThreadInfo<drogon_model::sqlite3::AiChats>(thread_id);

           if (!thread_info_opt.has_value())
           {
               LOG_ERROR << "can not get thread info: " << thread_id;
               return;
           }
			ChatMessage ai_response;

           ai_response.content =  Json::writeString(builder, ai_message);
           ai_response.thread_id = thread_id;
           const auto& thread_info = thread_info_opt.value();
           ai_response.sender_uid = std::to_string(thread_id);
           ai_response.sender_name = thread_info.getValueOfName();

            const auto& ai_record_opt = 
                BuildMessages(ai_response);

			if (!ai_record_opt.has_value())
            {
                LOG_ERROR << "can not build message";
                return;
            }

            LOG_INFO << "AI message: " << ai_record_opt.value().getValueOfContent();

			ThreadManager::PushChatRecords(user_message.value());
            ThreadManager::PushChatRecords(ai_record_opt.value());

				break;
			}
            default:
				LOG_ERROR<<"error thread type";
				break;
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "can not build message from json: " << e.what();
    }
}

void ChatController::handleNewConnection(const drogon::HttpRequestPtr& req,
	const drogon::WebSocketConnectionPtr& conn)
{
    std::string token;
    //1.find token in authorization
    std::string authHeader = req->getHeader("Authorization");
    if (!authHeader.empty()) {
        if (authHeader.substr(0, 4) == "JWT ") {
            token = authHeader.substr(4);
        }
        else {
            token = authHeader;
        }
    }
    // 2.find token in Json
    else if (auto jsonObj = req->getJsonObject(); jsonObj && jsonObj->isMember("token")) {
        token = (*jsonObj)["token"].asString();
    }

    // 3.find token in parameter
    else if (!req->getParameter("token").empty()) {
        token = req->getParameter("token");
    }

    Utils::UsersInfo info;
    if (!Utils::Authentication::VerifyJWT(token, info))
    {
        conn->send("Failed to verify token");
        conn->shutdown();
        return;
    }
    
    if (!ConnectionManager::GetInstance().AddConnection(info, conn))
    {
        LOG_ERROR << "can not add connection!";
    }
    else
    {
        LOG_INFO << "add new connection: "+info.ToString();
    }
}

void ChatController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    auto info_ptr = conn->getContext<Utils::UsersInfo>();
    if (info_ptr) {
		LOG_INFO << "username: " << info_ptr->username << " : connection close";
        ConnectionManager::GetInstance().RemoveConnection(info_ptr->uid);
    }
}

Json::Value ChatController::GenSyncResp(const std::string& resp)
{
    //target is to build json response like this: { "role" : " " , "content" : " " } 

    if (resp.find("error")!=std::string::npos
        && resp.find("code") != std::string::npos
        && resp.find("message") != std::string::npos)
    {
        LOG_ERROR << "response is error";
        return Json::nullValue;
    }

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(resp,root))
    {
        LOG_ERROR << "can not parse response";
        return Json::nullValue;
    }

    try
    {
        Json::Value json_resp;
        //maybe we need to verify if "content" is reasoning_content or content
        json_resp["content"] = root["content"];
        json_resp["role"] = "assistant";
        return json_resp;
    }
	catch (const std::exception& e){
        LOG_ERROR << "exception: " << e.what();
        return Json::nullValue;
	}

}

std::optional<drogon_model::sqlite3::Messages> ChatController::BuildMessages(const ChatMessage& message)
{
    drogon_model::sqlite3::Messages messages;
    if (!message.content.empty())
        messages.setContent(message.content);
    if (message.attachment != Json::nullValue)
    {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        messages.setAttachment(Json::writeString(builder,message.attachment));
    }
    else
    {
        messages.setAttachmentToNull();
    }
    messages.setThreadId(message.thread_id);
    messages.setSenderUid(message.sender_uid);
    messages.setSenderName(message.sender_name);
    messages.setSenderAvatar(message.sender_avatar);
    const auto& time = trantor::Date::now().toDbString();
    messages.setCreateTime(time);
    messages.setUpdateTime(time);

    return messages;
}

