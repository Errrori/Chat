#include "pch.h"
#include "FileController.h"
#include "../manager/FileManager.h"

using namespace drogon;

void FileController::UploadImage(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	MultiPartParser parser;
	//文件无法解析或者获取到的文件超过一个
	if (parser.parse(req)!=0||parser.getFiles().size()!=1)
	{
		Json::Value data;
		data["code"] = 400;
		data["message"] = "File can not parse or the number of upload files more than 1";
		callback(HttpResponse::newHttpJsonResponse(data));
		return;
	}

	auto& file = parser.getFiles()[0];

	if (file.getFileType()!=FT_IMAGE)
	{
		Json::Value data;
		data["code"] = 403;
		data["message"] = "Can not upload file that is not image here!";
		callback(HttpResponse::newHttpJsonResponse(data));
		return;
	}
	auto file_extension = std::string(file.getFileExtension());
	if (file_extension != "png" && file_extension != "jpg"
		&& file_extension != "jpeg" && file_extension != "gif"&&file_extension!="WebP")
	{
		LOG_ERROR << "upload file is not supported : "<<file_extension;
		Json::Value data;
		data["code"] = 400;
		data["message"] = "upload file is not supported";
		callback(HttpResponse::newHttpJsonResponse(data));
		return;
	}

	std::string new_file_name = Utils::GenerateUid() + "."+file_extension;
	std::string file_url;
	if (FileManager::UploadFile(file, new_file_name,file_url))
	{
		Json::Value data;
		data["code"] = 200;
		data["message"] = "Success add file";
		data["md5"] = file.getMd5();
		if (file_url.substr(0, 2) == "./") {
			file_url = file_url.substr(2);
		}
		std::replace(file_url.begin(), file_url.end(), '\\', '/');
		data["data"] = file_url;
		callback(HttpResponse::newHttpJsonResponse(data));
		return;
	}
	Json::Value data;
	data["code"] = 400;
	data["message"] = "fail to add file";
	callback(HttpResponse::newHttpJsonResponse(data));
}
