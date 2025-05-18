#ifndef CONST_H_
#define CONST_H_
namespace DataBase
{
	const static std::string USER_TABLE = "CREATE TABLE IF NOT EXISTS users("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"username TEXT NOT NULL,"
		"account TEXT NOT NULL,"
		"password TEXT NOT NULL,"
		"uid TEXT NOT NULL UNIQUE,"
		"avatar TEXT DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
		");";
	const static std::string CHAT_RECORDS_TABLE = "CREATE TABLE IF NOT EXISTS chat_records("
		"message_id BIGINT PRIMARY KEY,"
		"sender_name TEXT NOT NULL,"
		"sender_uid TEXT NOT NULL,"
		"content TEXT NOT NULL,"
		"avatar TEXT DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
		"message_type TEXT NOT NULL,"
		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
		");";
	constexpr static unsigned int DEFAULT_RECORDS_QUERY_LEN = 50;
}

namespace DeFaultFilePath
{
	const std::string DEFAULT_UPLOAD_PATH = "upload_files";
	const std::string UPLOAD_IMAGE_DIR = "image";
	const std::string UPLOAD_MEDIA_DIR = "media";
	const std::string UPLOAD_DOCUMENT_DIR = "document";
}

#endif
