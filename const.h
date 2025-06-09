#ifndef CONST_H_
#define CONST_H_
namespace DataBase
{
	const static std::string USER_TABLE = "CREATE TABLE IF NOT EXISTS users("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"username TEXT NOT NULL,"
		"account TEXT NOT NULL UNIQUE,"
		"password TEXT NOT NULL,"
		"uid TEXT NOT NULL UNIQUE,"
		"avatar TEXT DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
		"create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
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
		"create_time DEFAULT CURRENT_TIMESTAMP,"
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
	const std::string DEFAULT_UPLOAD_PATH = "upload_files";
	const std::string UPLOAD_IMAGE_DIR = "image";
	const std::string UPLOAD_MEDIA_DIR = "media";
	const std::string UPLOAD_DOCUMENT_DIR = "document";
}


static constexpr unsigned short MAX_MESSAGE_SEND_ONCE = 10;
static constexpr std::chrono::milliseconds MESSAGE_SEND_WAIT_TIME = std::chrono::milliseconds(1);

#endif
