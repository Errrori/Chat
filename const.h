#ifndef CONST_H_
#define CONST_H_
namespace DataBase
{
	// 优化的聊天系统表定义
	const static std::string THREAD_TABLE = "CREATE TABLE IF NOT EXISTS threads("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"type INTEGER NOT NULL," // 0=group, 1=private
		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
		");";

	// 私聊表 - 成为好友后生成对应的会话
	const static std::string PRIVATE_TABLE = "CREATE TABLE IF NOT EXISTS private_chats("
		"thread_id INTEGER PRIMARY KEY,"
		"uid1 TEXT NOT NULL,"
		"uid2 TEXT NOT NULL,"
		"FOREIGN KEY (thread_id) REFERENCES threads(id) ON DELETE CASCADE,"
		"FOREIGN KEY (uid1) REFERENCES users(uid) ON DELETE CASCADE,"
		"FOREIGN KEY (uid2) REFERENCES users(uid) ON DELETE CASCADE,"
		"UNIQUE (uid1, uid2)" // 防止重复的私聊会话
		");";
	//		"CHECK (uid1 < uid2)," // 确保uid1总是小于uid2，避免重复

	// 群聊表
	const static std::string GROUP_TABLE = "CREATE TABLE IF NOT EXISTS group_chats("
		"thread_id INTEGER PRIMARY KEY,"
		"name TEXT NOT NULL,"
		"avatar TEXT DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
		"description TEXT,"
		"FOREIGN KEY (thread_id) REFERENCES threads(id) ON DELETE CASCADE"
		");";

	// 群成员表
	const static std::string GROUP_MEMBER_TABLE = "CREATE TABLE IF NOT EXISTS group_members("
		"thread_id INTEGER NOT NULL,"
		"user_uid TEXT NOT NULL,"
		"role INTEGER NOT NULL DEFAULT 0," //(CHECK(role IN (0, 1, 2)) 0=member, 1=admin, 2=owner
		"join_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
		"FOREIGN KEY (thread_id) REFERENCES group_chats(thread_id) ON DELETE CASCADE,"
		"FOREIGN KEY (user_uid) REFERENCES users(uid) ON DELETE CASCADE,"
		"PRIMARY KEY (thread_id,user_uid),"
		"UNIQUE (thread_id, user_uid)" // 防止重复加入群聊
		");";

	// 消息表 - 包含用户信息冗余以提高查询性能
	const static std::string MESSAGE_TABLE = "CREATE TABLE IF NOT EXISTS messages("
		"message_id BIGINT PRIMARY KEY,"
		"thread_id INTEGER NOT NULL,"
		"sender_uid TEXT NOT NULL,"
		"sender_name TEXT NOT NULL,"
		"sender_avatar TEXT NOT NULL,"
		"content TEXT,"
		"attachment TEXT," // JSON格式存储附件信息
		"status INTEGER NOT NULL DEFAULT 1,"
		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
		"update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
		"FOREIGN KEY (thread_id) REFERENCES threads(id) ON DELETE CASCADE,"
		"FOREIGN KEY (sender_uid) REFERENCES users(uid) ON DELETE CASCADE"
		");";

	const static std::string USER_TABLE = "CREATE TABLE IF NOT EXISTS users("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"username TEXT NOT NULL,"
		"account TEXT NOT NULL UNIQUE,"
		"password TEXT NOT NULL,"
		"uid TEXT NOT NULL UNIQUE,"
		"avatar TEXT DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
		"signature TEXT,"
		"last_login_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
		"posts INTEGER DEFAULT 0,"
		"level INTEGER DEFAULT 0,"
		"status INTEGER DEFAULT 0,"
		"email TEXT,"
		"followers INTEGER DEFAULT 0,"
		"following INTEGER DEFAULT 0 "
		");";

	const static std::string CHAT_RECORDS_TABLE = "CREATE TABLE IF NOT EXISTS chat_records("
		"message_id BIGINT PRIMARY KEY,"
		"sender_uid TEXT,"
		"sender_name TEXT,"
		"sender_avatar TEXT,"
		"conversation_id TEXT,"
		"conversation_name TEXT,"
		"receiver_uid TEXT NOT NULL,"
		"content TEXT NOT NULL,"
		"content_type TEXT NOT NULL,"
		"chat_type TEXT NOT NULL,"
		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
		"FOREIGN KEY (sender_uid) REFERENCES users(uid) ON DELETE CASCADE,"
		"FOREIGN KEY (receiver_uid) REFERENCES users(uid) ON DELETE CASCADE"
		");";

	const static std::string RELATIONSHIPS_TABLE = "CREATE TABLE IF NOT EXISTS relationships("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"first_uid TEXT NOT NULL,"
		"second_uid TEXT NOT NULL,"
		"status TEXT NOT NULL,"
		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
		"update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
		"FOREIGN KEY (first_uid) REFERENCES users(uid) ON DELETE CASCADE,"
		"FOREIGN KEY (second_uid) REFERENCES users(uid) ON DELETE CASCADE,"
		"UNIQUE (first_uid,second_uid)"
		");";
	//STATUS : pending/blocking/friend/refuse/
	//situation Pending : first_uid is the uid of actor
	//situation Friend : first_uid is the smaller one among two users

	const static std::string GROUPS_TABLE = "CREATE TABLE IF NOT EXISTS groups("
		"group_id TEXT PRIMARY KEY,"
		"group_name TEXT NOT NULL,"
		"master_uid TEXT NOT NULL,"
		"group_avatar TEXT,"
		"description TEXT,"
		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
		"FOREIGN KEY (master_uid) REFERENCES users(uid) ON DELETE NO ACTION"
		");";

	const static std::string GROUP_MEMBERS_TABLE = "CREATE TABLE IF NOT EXISTS group_members ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"group_id TEXT NOT NULL,"
		"member_uid TEXT NOT NULL,"
		"role TEXT NOT NULL DEFAULT 'member',"
		"join_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
		"FOREIGN KEY (group_id) REFERENCES groups(group_id) ON DELETE CASCADE,"
		"FOREIGN KEY (member_uid) REFERENCES users(uid) ON DELETE CASCADE,"
		"UNIQUE (group_id, member_uid)"
		");";
	//ROLE: member,master,administrator

	const static std::string NOTIFICATION_TABLE = "CREATE TABLE IF NOT EXISTS notifications ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"notification_id TEXT NOT NULL UNIQUE,"
		"notification_type TEXT NOT NULL,"
		"sender_uid TEXT NOT NULL,"
		"receiver_uid TEXT NOT NULL,"
		"content TEXT,"
		"is_read BOOLEAN DEFAULT FALSE,"
		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
		"FOREIGN KEY (sender_uid) REFERENCES users(uid) ON DELETE CASCADE,"
		"FOREIGN KEY (receiver_uid) REFERENCES users(uid) ON DELETE CASCADE,"
		"UNIQUE (sender_uid, receiver_uid,notification_type)"
		");";

	const static std::string CREATE_INDEX_1 = "CREATE INDEX IF NOT EXISTS idx_chat_records_sender_receiver ON chat_records(sender_uid, receiver_uid);";
	const static std::string CREATE_INDEX_2 = "CREATE INDEX IF NOT EXISTS idx_chat_records_receiver_sender ON chat_records(receiver_uid, sender_uid);";
	const static std::string CREATE_INDEX_3 = "CREATE INDEX IF NOT EXISTS idx_chat_records_sender_conversation ON chat_records(sender_uid, conversation_id);";
	const static std::string CREATE_INDEX_4 = "CREATE INDEX IF NOT EXISTS idx_chat_records_conversation_sender ON chat_records(conversation_id, sender_uid);";
	const static std::string CREATE_INDEX_5 = "CREATE INDEX IF NOT EXISTS idx_relationships_first_second ON relationships (first_uid, second_uid);";
	const static std::string CREATE_INDEX_6 = "CREATE INDEX IF NOT EXISTS idx_relationships_second_first ON relationships(second_uid, first_uid);";

	const static std::string CREATE_TRIGGER = "CREATE TRIGGER IF NOT EXISTS update_relationships_timestamp "
		"AFTER UPDATE ON relationships "
		"FOR EACH ROW "
		"BEGIN "
		"UPDATE relationships SET update_time = CURRENT_TIMESTAMP WHERE id = NEW.id; "
		"END;";

	constexpr static unsigned int DEFAULT_RECORDS_QUERY_LEN = 50;
}

namespace DeFaultFilePath
{
	
}

static constexpr int PUBLIC_CHAT_ID = 1;

static constexpr unsigned short MAX_MESSAGE_SEND_ONCE = 10;
static constexpr std::chrono::milliseconds MESSAGE_SEND_WAIT_TIME = std::chrono::milliseconds(1);

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


//attachment
//{
//	"type":"image",
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


//	const static std::string THREAD_TABLE = "CREATE TABLE IF NOT EXISTS threads("
//		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
//		"type TINYINT NOT NULL,"
//		"created_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP";
//
//	const static std::string PRIVATE_TABLE = "CREATE TABLE IF NOT EXISTS private("
//		"thread_id INTEGER PRIMARY KEY,"
//		"uid1 TEXT NOT NULL,"
//		"uid2 TEXT NOT NULL";
//
//	const static std::string GROUP_TABLE = "CREATE TABLE IF NOT EXISTS groups("
//		"thread_id INTEGER PRIMARY KEY,"
//		"name TEXT NOT NULL,"
//		"avatar TEXT DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png'";
//
//	const static std::string GROUP_MEMBER = "CREATE TABLE IF NOT EXISTS members("
//		"thread_id INTEGER PRIMARY KEY,"
//		"user_uid TEXT NOT NULL,"
//		"role TINYINT NOT NULL DEFAULT 0,"
//		"joined_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP";
//
//	const static std::string MESSAGE_TABLE = "CREATE TABLE IF NOT EXISTS chat_records("
//		"message_id BIGINT PRIMARY KEY,"
//		"thread_id INTEGER,"
//		"sender_uid TEXT NOT NULL,"
//		"receiver_uid TEXT NOT NULL,"
//		"content TEXT,"
//		"attachment TEXT,"
//		"created_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP";
//
//	const static std::string USER_TABLE = "CREATE TABLE IF NOT EXISTS users("
//		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
//		"username TEXT NOT NULL,"
//		"account TEXT NOT NULL UNIQUE,"
//		"password TEXT NOT NULL,"
//		"uid TEXT NOT NULL UNIQUE,"
//		"avatar TEXT DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
//		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
//		");";
//
//	const static std::string CHAT_RECORDS_TABLE = "CREATE TABLE IF NOT EXISTS chat_records("
//		"message_id BIGINT PRIMARY KEY,"
//		"sender_uid TEXT,"
//		"sender_name TEXT,"
//		"sender_avatar TEXT,"
//		"conversation_id TEXT,"
//		"conversation_name TEXT,"
//		"receiver_uid TEXT NOT NULL,"
//		"content TEXT NOT NULL,"
//		"content_type TEXT NOT NULL,"
//		"chat_type TEXT NOT NULL,"
//		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
//		"FOREIGN KEY (sender_uid) REFERENCES users(uid) ON DELETE CASCADE," 
//		"FOREIGN KEY (receiver_uid) REFERENCES users(uid) ON DELETE CASCADE"
//		");";
//
//	const static std::string RELATIONSHIPS_TABLE = "CREATE TABLE IF NOT EXISTS relationships("
//		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
//		"first_uid TEXT NOT NULL,"
//		"second_uid TEXT NOT NULL,"
//		"status TEXT NOT NULL,"
//		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
//		"update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"  
//		"FOREIGN KEY (first_uid) REFERENCES users(uid) ON DELETE CASCADE,"   
//		"FOREIGN KEY (second_uid) REFERENCES users(uid) ON DELETE CASCADE,"
//		"UNIQUE (first_uid,second_uid)"
//		");";  
//	//STATUS : pending/blocking/friend/refuse/
//	//situation Pending : first_uid is the uid of actor
//	//situation Friend : first_uid is the smaller one among two users
//
//	const static std::string GROUPS_TABLE = "CREATE TABLE IF NOT EXISTS groups("
//		"group_id TEXT PRIMARY KEY,"
//		"group_name TEXT NOT NULL,"
//		"master_uid TEXT NOT NULL,"
//		"group_avatar TEXT,"
//		"description TEXT,"
//		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
//		"FOREIGN KEY (master_uid) REFERENCES users(uid) ON DELETE NO ACTION"
//		");";
//
//	const static std::string GROUP_MEMBERS_TABLE = "CREATE TABLE IF NOT EXISTS group_members ("
//		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
//		"group_id TEXT NOT NULL,"
//		"member_uid TEXT NOT NULL,"
//		"role TEXT NOT NULL DEFAULT 'member',"
//		"join_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
//		"FOREIGN KEY (group_id) REFERENCES groups(group_id) ON DELETE CASCADE,"
//		"FOREIGN KEY (member_uid) REFERENCES users(uid) ON DELETE CASCADE,"  
//		"UNIQUE (group_id, member_uid)"
//		");";
//	//ROLE: member,master,administrator
//
//	const static std::string NOTIFICATION_TABLE = "CREATE TABLE IF NOT EXISTS notifications ("
//		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
//		"notification_id TEXT NOT NULL UNIQUE,"
//		"notification_type TEXT NOT NULL,"
//		"sender_uid TEXT NOT NULL,"
//		"receiver_uid TEXT NOT NULL,"
//		"content TEXT,"
//		"is_read BOOLEAN DEFAULT FALSE,"
//		"create_time DEFAULT CURRENT_TIMESTAMP,"
//		"FOREIGN KEY (sender_uid) REFERENCES users(uid) ON DELETE CASCADE,"
//		"FOREIGN KEY (receiver_uid) REFERENCES users(uid) ON DELETE CASCADE,"
//		"UNIQUE (sender_uid, receiver_uid,notification_type)"
//		");";
//
//	const static std::string CREATE_INDEX_1 = "CREATE INDEX IF NOT EXISTS idx_chat_records_sender_receiver ON chat_records(sender_uid, receiver_uid);";
//	const static std::string CREATE_INDEX_2 = "CREATE INDEX IF NOT EXISTS idx_chat_records_receiver_sender ON chat_records(receiver_uid, sender_uid);";
//	const static std::string CREATE_INDEX_3 = "CREATE INDEX IF NOT EXISTS idx_chat_records_sender_conversation ON chat_records(sender_uid, conversation_id);";
//	const static std::string CREATE_INDEX_4 = "CREATE INDEX IF NOT EXISTS idx_chat_records_conversation_sender ON chat_records(conversation_id, sender_uid);";
//	const static std::string CREATE_INDEX_5 = "CREATE INDEX IF NOT EXISTS idx_relationships_first_second ON relationships (first_uid, second_uid);";
//	const static std::string CREATE_INDEX_6 = "CREATE INDEX IF NOT EXISTS idx_relationships_second_first ON relationships(second_uid, first_uid);";
//
//	//
//	const static std::string CREATE_INDEX_7 = "CREATE INDEX IF NOT EXISTS idx_threads_type ON threads(type);";
//	const static std::string CREATE_INDEX_8 = "CREATE INDEX IF NOT EXISTS idx_threads_created_time ON threads(created_time);";
//	// 私聊表索引
//	const static std::string CREATE_INDEX_9 = "CREATE INDEX IF NOT EXISTS idx_private_uid1 ON private_chats(uid1);";
//	const static std::string CREATE_INDEX_10 = "CREATE INDEX IF NOT EXISTS idx_private_uid2 ON private_chats(uid2);";
//	// 群聊表索引
//	const static std::string CREATE_INDEX_11 = "CREATE INDEX IF NOT EXISTS idx_group_name ON group_chats(name);";
//	// 群成员表索引
//	const static std::string CREATE_INDEX_12 = "CREATE INDEX IF NOT EXISTS idx_group_members_thread ON group_members(thread_id);";
//	const static std::string CREATE_INDEX_13 = "CREATE INDEX IF NOT EXISTS idx_group_members_user ON group_members(user_uid);";
//	const static std::string CREATE_INDEX_14 = "CREATE INDEX IF NOT EXISTS idx_group_members_role ON group_members(thread_id, role);";
//	// 消息表索引 - 最重要的性能优化
//	const static std::string CREATE_INDEX_15 = "CREATE INDEX IF NOT EXISTS idx_messages_thread_time ON messages(thread_id, created_time);";
//	const static std::string CREATE_INDEX_16 = "CREATE INDEX IF NOT EXISTS idx_messages_thread_id ON messages(thread_id, message_id);";
//	const static std::string CREATE_INDEX_17 = "CREATE INDEX IF NOT EXISTS idx_messages_sender ON messages(sender_uid);";
//
//	const static std::string CREATE_TRIGGER = "CREATE TRIGGER IF NOT EXISTS update_relationships_timestamp " 
//	    "AFTER UPDATE ON relationships "
//	    "FOR EACH ROW "
//	    "BEGIN "
//	    "UPDATE relationships SET update_time = CURRENT_TIMESTAMP WHERE id = NEW.id; "
//	    "END;";
//
//	constexpr static unsigned int DEFAULT_RECORDS_QUERY_LEN = 50;
//}
//
//namespace DeFaultFilePath
//{
//	const std::string DEFAULT_UPLOAD_PATH = "upload_files";
//	const std::string UPLOAD_IMAGE_DIR = "image";
//	const std::string UPLOAD_MEDIA_DIR = "media";
//	const std::string UPLOAD_DOCUMENT_DIR = "document";
//}
//
//
//static constexpr unsigned short MAX_MESSAGE_SEND_ONCE = 10;
//static constexpr std::chrono::milliseconds MESSAGE_SEND_WAIT_TIME = std::chrono::milliseconds(1);
//
//#endif
