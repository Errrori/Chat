#include "pch.h"
#include <filesystem>
#include "FileManager.h"

using namespace DeFaultFilePath;

bool FileManager::UploadFile(const drogon::HttpFile& file, const std::string& file_name, std::string& file_url)
{
	//获取默认上传路径
	auto upload_path = drogon::app().getUploadPath();
	if (upload_path.empty()) upload_path = DEFAULT_UPLOAD_PATH;
	//判断文件类型对应的存储文件夹
	std::string target_dir;
	switch (file.getFileType())
	{
	case drogon::FT_IMAGE:
		target_dir = UPLOAD_IMAGE_DIR;
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
