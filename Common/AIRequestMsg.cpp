#include "pch.h"
#include "AIRequestMsg.h"

std::string RequestMsg::RoleToString(Role role)
{
	switch (role)
	{
	case Role::User:
		return "user";
	case Role::Assistant:
		return "assistant";
	case Role::Tool:
		return "tool";
	default:
		return "";
	}
}

std::string RequestMsg::ModelToString(Model model)
{
	switch (model)
	{
	case Model::glm_flash:
		return "glm-4.5-flash";
	case Model::glm:
		return "glm-4.6";
	default:
		return "";
	}
}


Json::Value RequestMsg::ToJsonReq() const
{
	if (!IsValid())
		return Json::nullValue;

	try
	{
		Json::Value req;
		req["model"] = ModelToString(_model);
		req["max_tokens"] = _max_tokens;
		req["stream"] = _is_stream;
		req["do_sample"] = _do_sample;
		Json::Value message = Json::arrayValue;
		if (_context.has_value())
		{
			for (const auto& record : _context.value())
				message.append(record);
		}
		if (!_is_thinking)
		{
			Json::Value thinking;
			thinking["type"] = "disabled";
			req["thinking"] = thinking;
		}
		Json::Value msg;
		msg["role"] = RoleToString(_role);
		msg["content"] = _content;
		message.append(msg);
		req["messages"] = message;
		return req;
	}catch (const std::exception& e)
	{
		LOG_ERROR << "exception in ToJsonReq: " << e.what();
		return Json::nullValue;
	}
	
}

void RequestMsg::SetContext(const Json::Value& context)
{
	if (context.empty())
	{
		LOG_INFO << "context is empty";
		return;
	}

	try
	{
		_context.reset();
		_context.emplace(Json::Value(Json::arrayValue));
		Json::Value ctx;
		for (const auto& msg : context)
		{
			ctx["content"] = msg["content"];
			ctx["role"] = msg["role"];
			_context->append(ctx);
		}
	}catch (std::exception& e)
	{
		LOG_ERROR << "exception in RequestMsg::SetContext: " << e.what();
		_context.reset();
	}
}

bool RequestMsg::IsValid() const
{
	return !RoleToString(_role).empty() && !ModelToString(_model).empty()
		&& !_content.empty() && !_request_id.empty();
}
