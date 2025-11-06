#pragma once
#include <drogon/drogon.h>

class FileService
{
public:
	FileService() = delete;
	FileService(FileService&) = delete;
	FileService(FileService&&) = delete;

	static bool UploadFile(const drogon::HttpFile& file, const std::string& file_name, std::string& file_url);

	static bool CheckFileExists(const std::string& file_path);

};

