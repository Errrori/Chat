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

		if (name.empty()) std::cout << "username is empty!\n";
		if (password.empty()) std::cout << "password is empty!\n";


		auto uid = Utils::GenerateUid();
		if (!Users::AddUser(name, password, uid))
		{
			auto resp = drogon::HttpResponse::newHttpResponse();
			resp->setStatusCode(drogon::HttpStatusCode::k404NotFound);
			callback(resp);
			return;
		}
		Json::Value data;
		data["uid"] = uid;
		auto resp = drogon::HttpResponse::newHttpJsonResponse(data);
		resp->setStatusCode(drogon::HttpStatusCode::k200OK);
		resp->addHeader("content-type", "application/json");
		callback(resp);
	}
	else
	{
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
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
	std::string uid;
	auto data = req->getJsonObject();
	if (data) {
		name = (*data)["username"].asString();
		password = (*data)["password"].asString();
		uid = (*data)["uid"].asString();
		if (name.empty()) std::cout << "username is empty!\n";
		if (password.empty()) std::cout << "password is empty!\n";
		if (uid.empty()) std::cout << "uid is empty!\n";
	}
	password = Utils::PasswordHashed(password);

	Json::Value s_data;
	if (!Users::GetUserInfo(uid, s_data))
	{
		std::cout << "cam not find user info\n";
		auto resp = drogon::HttpResponse::newHttpResponse();
		resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
		callback(resp);
		return;
	}

	auto s_name = s_data["username"].asString();
	auto s_password = s_data["password"].asString();
	auto s_uid = s_data["uid"].asString();

	if (s_name!=name || s_password!=password || s_uid!=uid)
	{
		auto resp = drogon::HttpResponse::newHttpResponse();
		std::cout << "error to verify\n";
		resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
		callback(resp);
	}
	else
	{
		Json::Value data;
		data["jwt"] = "this is a monkey token";
		auto resp = drogon::HttpResponse::newHttpJsonResponse(data);
		resp->setStatusCode(drogon::k200OK);
		callback(resp);
	}
}

