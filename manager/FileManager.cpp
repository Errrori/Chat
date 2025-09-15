#include "pch.h"
#include <filesystem>
#include "FileManager.h"


bool FileManager::UploadFile(const drogon::HttpFile& file, const std::string& file_name, std::string& file_url)
{
	//获取默认上传路径
	auto upload_path = drogon::app().getUploadPath();
	if (upload_path.empty()) upload_path = UploadsFile::DEFAULT_UPLOAD_PATH;
	//判断文件类型对应的存储文件夹
	std::string target_dir;
	switch (file.getFileType())
	{
	case drogon::FT_IMAGE:
		target_dir = UploadsFile::UPLOAD_IMAGE_DIR;
		break;
	default:
		LOG_ERROR << "Unsupported file type";
		return false;
	}
	//生成对应的存储文件夹(不是完整路径)
	std::filesystem::path base_path(upload_path);
	std::filesystem::path target_path(target_dir);
	std::filesystem::path store_dir_fs = base_path / target_path;
	auto store_dir = store_dir_fs.string();

	//如果文件夹不存在则新建一个文件夹
	if (!std::filesystem::exists(store_dir))
	{
		try
		{
			std::filesystem::create_directories(store_dir);
		}catch (const std::filesystem::filesystem_error& e)
		{
			LOG_ERROR << "Exception to add file:"<<e.what();
			return false;
		}
	}
	
	//生成最终的存储位置
	std::filesystem::path final_store_path = store_dir_fs / file_name;
	auto store_path = final_store_path.string();

	if (file.saveAs(store_path)==0)
	{
		file_url = store_path;
		return true;
	}
	//生成失败则不更新文件的url
	return false;
}

bool FileManager::CheckFileExists(const std::string& file_path)
{
	if (file_path.length()>=UploadsFile::FILE_LEN_CHECK_LIMIT)
	{
		LOG_ERROR << "file path is too long";
		return false;
	}
	if (file_path.find("..") != std::string::npos ||
		file_path.find("\\") != std::string::npos)
	{
		LOG_ERROR << "path contains illegal characters";
		return false;
	}

	size_t first_slash = file_path.find('/');
	if (first_slash != std::string::npos) {
		if (file_path.substr(0, first_slash)!=UploadsFile::DEFAULT_UPLOAD_PATH)
		{
			LOG_ERROR << "file path does not start with default upload path";
			return false;
		}
	}
	else
	{
		LOG_ERROR << "file path does not start with default upload path";
		return false;
	}

	//temporary check file extension only for image files
	size_t last_dot = file_path.find_last_of('.');
	std::string file_extension;
	if (last_dot != std::string::npos) {
		file_extension = file_path.substr(last_dot + 1);
	}
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
		LOG_ERROR << "File type is not supported: " << file_extension;
		return false;
	}

	//need to check more characters that are invalid

	return true;
}
