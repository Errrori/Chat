#pragma once
#include <drogon/drogon.h>

class FileManager
{
public:
	FileManager() = delete;
	FileManager(FileManager&) = delete;
	FileManager(FileManager&&) = delete;

	static bool UploadFile(const drogon::HttpFile& file, const std::string& file_name, std::string& file_url);
};

