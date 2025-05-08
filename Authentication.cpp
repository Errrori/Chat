#include "Authentication.h"
#include "Users.h"
#include "Utils.h"

void Authentication::Controller::HandleRegister(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	std::cout << "access the Register route\n";
	std::cout << "data is : "<<req->bodyData() << std::endl;

	std::string name;
	std::string password;
	auto data = req->getJsonObject();
	if (data) {
		name = (*data)["username"].asString();
		password = (*data)["password"].asString();

		if (name.empty()||password.empty())
		{
			Json::Value data;
			data["code"] = 400;
			data["message"] = "name or password is empty";
			auto resp = drogon::HttpResponse::newHttpJsonResponse(data);
			resp->addHeader("content-type", "application/json");
			callback(resp);
			return;
		}

		auto uid = Utils::GenerateUid();
		if (!Users::AddUser(name, password, uid))
		{
			Json::Value data;
			data["code"] = 501;
			data["message"] = "Server Database error: can not add user";
			auto resp = drogon::HttpResponse::newHttpJsonResponse(data);
			resp->addHeader("content-type", "application/json");
			callback(resp);
			return;
		}
		Json::Value data;
		data["uid"] = uid;
		data["code"] = 200;
		auto resp = drogon::HttpResponse::newHttpJsonResponse(data);
		resp->addHeader("content-type", "application/json");
		callback(resp);
	}
	else
	{
		Json::Value data;
		data["code"] = 400;
		data["message"] = "request body is empty";
		auto resp = drogon::HttpResponse::newHttpJsonResponse(data);
		resp->addHeader("content-type", "application/json");
		callback(resp);
	}

}

void Authentication::Controller::HandleLogin(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	std::cout << "access the Login route\n";
	std::cout << "data is : " << req->bodyData() << std::endl;
	std::string name;	
	std::string password;
	auto data = req->getJsonObject();
	if (data) {
		name = (*data)["username"].asString();
		password = (*data)["password"].asString();
		if (name.empty()) std::cout << "username is empty!\n";
		if (password.empty()) std::cout << "password is empty!\n";
	}
	password = Utils::PasswordHashed(password);

	Json::Value s_data;
	if (!Users::GetUserInfoByName(name, s_data))
	{
		std::cout << "cam not find user info\n";
		Json::Value data;
		data["code"] = 400;
		data["message"] = "Can not find user!";
		auto resp = drogon::HttpResponse::newHttpJsonResponse(data);
		resp->addHeader("content-type", "application/json");
		callback(resp);
		return;
	}

	auto s_name = s_data["username"].asString();
	auto s_password = s_data["password"].asString();
	auto s_uid = s_data["uid"].asString();

	if (s_name!=name || s_password!=password )
	{
		Json::Value data;
		data["code"] = 401;
		if (s_name!=name)
			data["message"] = "username is no correct";
		if (s_password!=password)
			data["message"] = "password is no correct";
		
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->addHeader("content-type", "application/json");
		std::cout << "error to verify\n";
		callback(resp);
	}
	else
	{
		Json::Value data;
		data["message"] = "Login success";
		data["token"] = "this is a monkey token";
		data["code"] = 200;
		data["uid"] = s_data["uid"].asString();
		auto resp = drogon::HttpResponse::newHttpJsonResponse(data);
		resp->addHeader("content-type", "application/json");
		callback(resp);
	}
}

