#pragma once
#include <drogon/HttpController.h>

class FileController:public drogon::HttpController<FileController>
{
public:
	METHOD_LIST_BEGIN
		ADD_METHOD_TO(FileController::UploadImage,"/file/upload/image",drogon::Post, "CorsMiddleware");
		
	METHOD_LIST_END

	void UploadImage(const drogon::HttpRequestPtr& req,
		std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

