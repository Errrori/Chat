#ifndef CONST_H_
#define CONST_H_
namespace DataBase
{
	const static std::string THREAD_TABLE = "CREATE TABLE IF NOT EXISTS threads("
		"thread_id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"type INTEGER NOT NULL," // 0=group, 1=private,2=ai
		"create_time INTEGER DEFAULT (strftime('%s','now'))"
		");";

	const static std::string AI_TABLE = "CREATE TABLE IF NOT EXISTS ai_chats("
		"thread_id INTEGER NOT NULL,"
		"name TEXT NOT NULL,"
		"creator_uid TEXT,"
		"avatar TEXT DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
		"init_settings TEXT,"
		"FOREIGN KEY (thread_id) REFERENCES threads(thread_id) ON DELETE CASCADE,"
		"FOREIGN KEY (creator_uid) REFERENCES users(uid) ON DELETE CASCADE"
		");";

	// 私聊表 - 成为好友后生成对应的会话
	const static std::string PRIVATE_TABLE = "CREATE TABLE IF NOT EXISTS private_chats("
		"thread_id INTEGER NOT NULL,"
		"uid1 TEXT NOT NULL,"
		"uid2 TEXT NOT NULL,"
		"FOREIGN KEY (thread_id) REFERENCES threads(thread_id) ON DELETE CASCADE,"
		"FOREIGN KEY (uid1) REFERENCES users(uid) ON DELETE CASCADE,"
		"FOREIGN KEY (uid2) REFERENCES users(uid) ON DELETE CASCADE,"
		"UNIQUE (uid1, uid2)" // 防止重复的私聊会话
		");";
	//		"CHECK (uid1 < uid2)," // 确保uid1总是小于uid2，避免重复

	// 群聊表
	const static std::string GROUP_TABLE = "CREATE TABLE IF NOT EXISTS group_chats("
		"thread_id INTEGER NOT NULL,"
		"name TEXT NOT NULL,"
		"avatar TEXT DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
		"description TEXT,"
		"FOREIGN KEY (thread_id) REFERENCES threads(thread_id) ON DELETE CASCADE"
		");";

	// 群成员表
	const static std::string GROUP_MEMBER_TABLE = "CREATE TABLE IF NOT EXISTS group_members("
		"thread_id INTEGER NOT NULL,"
		"user_uid TEXT NOT NULL,"
		"role INTEGER NOT NULL DEFAULT 0," //(CHECK(role IN (0, 1, 2)) 0=member, 1=admin, 2=owner
		"join_time INTEGER DEFAULT (strftime('%s','now')),"
		"FOREIGN KEY (thread_id) REFERENCES group_chats(thread_id) ON DELETE CASCADE,"
		"FOREIGN KEY (user_uid) REFERENCES users(uid) ON DELETE CASCADE,"
		"PRIMARY KEY (thread_id,user_uid),"
		"UNIQUE (thread_id, user_uid)" // 防止重复加入群聊
		");";

	// 消息表 - 包含用户信息冗余以提高查询性能
	const static std::string MESSAGE_TABLE = "CREATE TABLE IF NOT EXISTS messages("
		"message_id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"thread_id INTEGER NOT NULL,"
		"sender_uid TEXT NOT NULL,"
		"sender_name TEXT NOT NULL,"
		"sender_avatar TEXT NOT NULL,"
		"content TEXT,"
		"attachment TEXT," // JSON格式存储附件信息
		"status INTEGER NOT NULL DEFAULT 1,"
		"create_time INTEGER DEFAULT (strftime('%s','now')),"
		"update_time INTEGER DEFAULT (strftime('%s','now')),"
		"FOREIGN KEY (thread_id) REFERENCES threads(thread_id) ON DELETE CASCADE,"
		"FOREIGN KEY (sender_uid) REFERENCES users(uid) ON DELETE CASCADE"
		");";

	const static std::string AI_MESSAGE_TABLE = "CREATE TABLE IF NOT EXISTS ai_context("
		"message_id TEXT PRIMARY KEY,"
		"thread_id INTEGER NOT NULL,"
		"role TEXT NOT NULL,"
		"content TEXT NOT NULL,"
		"attachment TEXT,"
		"reasoning_content TEXT,"
		"created_time INTEGER DEFAULT (strftime('%s','now'))"
		");";

	const static std::string USER_TABLE = "CREATE TABLE IF NOT EXISTS users("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"username TEXT NOT NULL,"
		"account TEXT NOT NULL UNIQUE,"
		"password TEXT NOT NULL,"
		"uid TEXT NOT NULL UNIQUE,"
		"avatar TEXT DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
		"create_time INTEGER DEFAULT (strftime('%s','now')),"
		"signature TEXT,"
		"last_login_time INTEGER DEFAULT (strftime('%s','now')),"
		"posts INTEGER DEFAULT 0,"
		"level INTEGER DEFAULT 0,"
		"status INTEGER DEFAULT 0,"
		"email TEXT,"
		"followers INTEGER DEFAULT 0,"
		"following INTEGER DEFAULT 0 "
		");";

	const static std::vector<std::string> db_table_list = {
		THREAD_TABLE,
		AI_TABLE,
		PRIVATE_TABLE,
		GROUP_TABLE,
		GROUP_MEMBER_TABLE,
		MESSAGE_TABLE,
		USER_TABLE,
		AI_MESSAGE_TABLE
	};


	static const std::string idx1 = "CREATE INDEX idx_messages_thread_msg ON messages(thread_id, message_id)";

	
	static const std::string idx2 = "CREATE INDEX idx_private_uid1 ON private_chats(uid1)";
	static const std::string idx3 = "CREATE INDEX idx_private_uid2 ON private_chats(uid2)";
	static const std::string idx4 = "CREATE INDEX idx_group_members_uid ON group_members(user_uid)";

	constexpr static unsigned int DEFAULT_RECORDS_QUERY_LEN = 50;
}

namespace UploadsFile
{
	static const std::vector<std::string>
		UploadImageType = {"png","jpg","jpeg","gif","WebP"};

	static constexpr size_t MAX_PATH_LENGTH = 255;

	const std::string DEFAULT_UPLOAD_PATH = "uploads";
	const std::string UPLOAD_IMAGE_DIR = "image";
	const std::string UPLOAD_MEDIA_DIR = "media";
	const std::string UPLOAD_DOCUMENT_DIR = "document";
	constexpr int FILE_LEN_CHECK_LIMIT = 255;
}

namespace ChatCode
{
	enum Code
	{
		SystemException = -1,
		Unknown = 0,
		OK = 100,
		InValidJson = 101,
		MissingField = 102,
		BadDbOp = 103,
		FailAccessInfo = 104,
		NotPermission = 105,
		BadAIRequest = 106,
		FailAddConn = 107,
		UnMatchedType = 108,//thread type is not matched to thread_id
		LoginKicked = 109,//logged in from another device
	};
}




//attachment
//{
//	"_type":"image",
//	"file_path":"upload/image/file",
//	"metadata" (可选) :
//	{
//		"width": 图片 / 视频宽度,
//		"height" : 图片 / 视频高度,
//		"size": 文件大小
//	},
//	"format":"jpg",
//}

#endif