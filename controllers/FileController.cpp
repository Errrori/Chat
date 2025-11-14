#include "pch.h"
#include "FileController.h"
#include "Service/FileService.h"

using namespace drogon;

void FileController::UploadImage(const drogon::HttpRequestPtr& req,
	std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
	MultiPartParser parser;
	//File parsing issues or multiple files uploaded
	if (parser.parse(req)!=0||parser.getFiles().size()!=1)
	{
		auto resp = Utils::CreateErrorResponse(400, 400, "File cannot be parsed or number of upload files exceeds 1");
		callback(resp);
		return;
	}

	auto& file = parser.getFiles()[0];

	if (file.getFileType()!=FT_IMAGE)
	{
		auto resp = Utils::CreateErrorResponse(403, 403, "Cannot upload non-image files here");
		callback(resp);
		return;
	}
	auto file_extension = std::string(file.getFileExtension());
	std::transform(file_extension.begin(), file_extension.end(), file_extension.begin(), ::tolower);
	bool is_supported = false;
	for (const auto& type : UploadsFile::UploadImageType)
	{
		if (file_extension == type)
		{
			is_supported = true;
			break;
		}
	}
	if (!is_supported)
	{
		LOG_ERROR << "upload file is not supported : "<<file_extension;
		auto resp = Utils::CreateErrorResponse(400, 400, "Unsupported file type");
		callback(resp);
		return;
	}

	std::string new_file_name = Utils::Authentication::GenerateUid() + "."+file_extension;
	std::string file_url;
	if (FileService::UploadFile(file, new_file_name,file_url))
	{
		Json::Value data;
		data["code"] = 200;
		data["message"] = "File uploaded successfully";
		data["md5"] = file.getMd5();
		if (file_url.substr(0, 2) == "./") {
			file_url = file_url.substr(2);
		}
		std::replace(file_url.begin(), file_url.end(), '\\', '/');
		data["data"] = file_url;
		callback(HttpResponse::newHttpJsonResponse(data));
		return;
	}
	auto resp = Utils::CreateErrorResponse(500, 500, "Failed to upload file");
	callback(resp);
}
