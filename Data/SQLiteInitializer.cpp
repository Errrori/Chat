#include "pch.h"
#include "SQLiteInitializer.h"

// ---------------------------------------------------------------------------
// Versioned migration registry
//
// HOW TO ADD A SCHEMA CHANGE:
//   1. Append a new Migration entry at the END of this list.
//   2. Give it the next sequential version number.
//   3. Write the SQL that performs the change (ALTER TABLE, CREATE INDEX, …).
//   4. Never modify or reorder existing entries – they are history.
//
// On first deployment all migrations run in order from version 1.
// On upgrade only the new entries (version > stored version) run.
// ---------------------------------------------------------------------------

namespace {

struct Migration
{
    int         version;
    std::string description;
    std::string sql;          // may contain multiple statements separated by ";"
};

// ---------------------------------------------------------------------------
// SQLite-specific DDL.
// Note: strftime('%s','now') and AUTOINCREMENT are SQLite-only syntax.
//       PostgreSQL equivalents live in PostgreSQLInitializer.
// ---------------------------------------------------------------------------
const std::vector<Migration> MIGRATIONS = {
    // --- v1: base schema -------------------------------------------------------
    {
        1, "Create threads table",
        "CREATE TABLE IF NOT EXISTS threads("
        "  thread_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  type      INTEGER NOT NULL,"             // 0=group, 1=private, 2=ai
        "  create_time INTEGER DEFAULT (strftime('%s','now'))"
        ");"
    },
    {
        2, "Create users table",
        "CREATE TABLE IF NOT EXISTS users("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username    TEXT    NOT NULL,"
        "  account     TEXT    NOT NULL UNIQUE,"
        "  password    TEXT    NOT NULL,"
        "  uid         TEXT    NOT NULL UNIQUE,"
        "  avatar      TEXT    DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
        "  create_time INTEGER DEFAULT (strftime('%s','now')),"
        "  signature   TEXT,"
        "  email       TEXT"
        ");"
    },
    {
        3, "Create ai_chats table",
        "CREATE TABLE IF NOT EXISTS ai_chats("
        "  thread_id     INTEGER NOT NULL,"
        "  name          TEXT    NOT NULL,"
        "  creator_uid   TEXT,"
        "  avatar        TEXT    DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
        "  init_settings TEXT,"
        "  FOREIGN KEY (thread_id)   REFERENCES threads(thread_id) ON DELETE CASCADE,"
        "  FOREIGN KEY (creator_uid) REFERENCES users(uid)         ON DELETE CASCADE"
        ");"
    },
    {
        4, "Create private_chats table",
        "CREATE TABLE IF NOT EXISTS private_chats("
        "  thread_id INTEGER NOT NULL,"
        "  uid1      TEXT    NOT NULL,"
        "  uid2      TEXT    NOT NULL,"
        "  FOREIGN KEY (thread_id) REFERENCES threads(thread_id) ON DELETE CASCADE,"
        "  FOREIGN KEY (uid1)      REFERENCES users(uid)         ON DELETE CASCADE,"
        "  FOREIGN KEY (uid2)      REFERENCES users(uid)         ON DELETE CASCADE,"
        "  UNIQUE (uid1, uid2)"
        ");"
    },
    {
        5, "Create group_chats table",
        "CREATE TABLE IF NOT EXISTS group_chats("
        "  thread_id   INTEGER NOT NULL,"
        "  name        TEXT    NOT NULL,"
        "  avatar      TEXT    DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
        "  description TEXT,"
        "  FOREIGN KEY (thread_id) REFERENCES threads(thread_id) ON DELETE CASCADE"
        ");"
    },
    {
        6, "Create group_members table",
        "CREATE TABLE IF NOT EXISTS group_members("
        "  thread_id INTEGER NOT NULL,"
        "  user_uid  TEXT    NOT NULL,"
        "  role      INTEGER NOT NULL DEFAULT 0,"   // 0=member, 1=admin, 2=owner
        "  join_time INTEGER DEFAULT (strftime('%s','now')),"
        "  FOREIGN KEY (thread_id) REFERENCES threads(thread_id) ON DELETE CASCADE,"
        "  FOREIGN KEY (user_uid)  REFERENCES users(uid)         ON DELETE CASCADE,"
        "  PRIMARY KEY (thread_id, user_uid),"
        "  UNIQUE (thread_id, user_uid)"
        ");"
    },
    {
        7, "Create messages table",
        "CREATE TABLE IF NOT EXISTS messages("
        "  message_id    INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  thread_id     INTEGER NOT NULL,"
        "  sender_uid    TEXT    NOT NULL,"
        "  sender_name   TEXT    NOT NULL,"
        "  sender_avatar TEXT    NOT NULL,"
        "  content       TEXT,"
        "  attachment    TEXT,"                     // JSON attachment descriptor
        "  status        INTEGER NOT NULL DEFAULT 1,"
        "  create_time   INTEGER DEFAULT (strftime('%s','now')),"
        "  update_time   INTEGER DEFAULT (strftime('%s','now')),"
        "  FOREIGN KEY (thread_id)  REFERENCES threads(thread_id) ON DELETE CASCADE,"
        "  FOREIGN KEY (sender_uid) REFERENCES users(uid)         ON DELETE CASCADE"
        ");"
    },
    {
        8, "Create ai_context table",
        "CREATE TABLE IF NOT EXISTS ai_context("
        "  message_id        TEXT    PRIMARY KEY,"
        "  thread_id         INTEGER NOT NULL,"
        "  role              TEXT    NOT NULL,"
        "  content           TEXT    NOT NULL,"
        "  attachment        TEXT,"
        "  reasoning_content TEXT,"
        "  created_time      INTEGER DEFAULT (strftime('%s','now'))"
        ");"
    },
    {
        9, "Create notifications table",
        "CREATE TABLE IF NOT EXISTS notifications("
        "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  type          INTEGER NOT NULL CHECK (type IN (0,1,2,3)),"
        // 0=RequestReceived, 1=RequestAccepted, 2=RequestRejected, 3=BlockUser
        "  sender_uid    TEXT NOT NULL,"
        "  recipient_uid TEXT NOT NULL,"
        "  payload       TEXT,"
        "  is_read       INTEGER CHECK (is_read IN (0,1)) DEFAULT 0,"
        "  created_time  INTEGER DEFAULT (strftime('%s','now')),"
        "  FOREIGN KEY (sender_uid)    REFERENCES users(uid) ON DELETE CASCADE,"
        "  FOREIGN KEY (recipient_uid) REFERENCES users(uid) ON DELETE CASCADE"
        ");"
    },
    {
        10, "Create friendships table",
        "CREATE TABLE IF NOT EXISTS friendships("
        "  uid1         TEXT NOT NULL,"
        "  uid2         TEXT NOT NULL,"
        "  created_time INTEGER DEFAULT (strftime('%s','now')),"
        "  UNIQUE (uid1, uid2),"
        "  CHECK  (uid1 < uid2),"
        "  FOREIGN KEY (uid1) REFERENCES users(uid) ON DELETE CASCADE,"
        "  FOREIGN KEY (uid2) REFERENCES users(uid) ON DELETE CASCADE"
        ");"
    },
    {
        11, "Create friend_requests table",
        "CREATE TABLE IF NOT EXISTS friend_requests("
        "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  requester_uid TEXT    NOT NULL,"
        "  target_uid    TEXT    NOT NULL,"
        "  status        INTEGER NOT NULL CHECK (status IN (0,1,2)),"
        // 0=pending, 1=accepted, 2=refused
        "  created_time  INTEGER DEFAULT (strftime('%s','now')),"
        "  updated_time  INTEGER DEFAULT (strftime('%s','now')),"
        "  payload       TEXT,"
        "  FOREIGN KEY (requester_uid) REFERENCES users(uid) ON DELETE CASCADE,"
        "  FOREIGN KEY (target_uid)    REFERENCES users(uid) ON DELETE CASCADE"
        ");"
    },
    {
        12, "Create block table",
        "CREATE TABLE IF NOT EXISTS block("
        "  operator_uid TEXT    NOT NULL,"
        "  blocked_uid  TEXT    NOT NULL,"
        "  created_time INTEGER DEFAULT (strftime('%s','now')),"
        "  UNIQUE (operator_uid, blocked_uid),"
        "  FOREIGN KEY (operator_uid) REFERENCES users(uid) ON DELETE CASCADE,"
        "  FOREIGN KEY (blocked_uid)  REFERENCES users(uid) ON DELETE CASCADE"
        ");"
    },
    // --- v13+: indexes --------------------------------------------------------
    {
        13, "Index: messages(thread_id, message_id)",
        "CREATE INDEX IF NOT EXISTS idx_messages_thread_msg ON messages(thread_id, message_id);"
    },
    {
        14, "Index: private_chats(uid1)",
        "CREATE INDEX IF NOT EXISTS idx_private_uid1 ON private_chats(uid1);"
    },
    {
        15, "Index: private_chats(uid2)",
        "CREATE INDEX IF NOT EXISTS idx_private_uid2 ON private_chats(uid2);"
    },
    {
        16, "Index: group_members(user_uid)",
        "CREATE INDEX IF NOT EXISTS idx_group_members_uid ON group_members(user_uid);"
    },

    // ---- Append future migrations BELOW this line ----------------------------
    // Example:
    // { 17, "Add last_seen column to users",
    //   "ALTER TABLE users ADD COLUMN last_seen INTEGER DEFAULT 0;"
    // },
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// SQLiteInitializer implementation
// ---------------------------------------------------------------------------

SQLiteInitializer::SQLiteInitializer(drogon::orm::DbClientPtr db)
    : _db(std::move(db))
{
}

void SQLiteInitializer::Initialize()
{
    // SQLite-specific performance / safety settings
    Exec("PRAGMA journal_mode = WAL;");
    Exec("PRAGMA busy_timeout = 5000;");
    Exec("PRAGMA foreign_keys = ON;");

    // Ensure the migration tracking table exists before anything else
    EnsureMigrationTable();
}

void SQLiteInitializer::Migrate()
{
    const int currentVersion = GetCurrentVersion();
    LOG_INFO << "[SQLiteInitializer] Current schema version: " << currentVersion;

    for (const auto& m : MIGRATIONS)
    {
        if (m.version <= currentVersion)
            continue;

        LOG_INFO << "[SQLiteInitializer] Applying migration v" << m.version
                 << ": " << m.description;
        Exec(m.sql);
        SetVersion(m.version);
    }

    LOG_INFO << "[SQLiteInitializer] Schema is up to date (version "
             << GetCurrentVersion() << ").";
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void SQLiteInitializer::EnsureMigrationTable()
{
    Exec(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "  version    INTEGER PRIMARY KEY,"
        "  applied_at INTEGER DEFAULT (strftime('%s','now'))"
        ");"
    );
}

int SQLiteInitializer::GetCurrentVersion()
{
    try
    {
        auto result = _db->execSqlSync(
            "SELECT COALESCE(MAX(version), 0) AS v FROM schema_migrations;");
        if (!result.empty())
            return result[0]["v"].as<int>();
    }
    catch (const std::exception& e)
    {
        LOG_WARN << "[SQLiteInitializer] Could not read schema version: " << e.what();
    }
    return 0;
}

void SQLiteInitializer::SetVersion(int version)
{
    try
    {
        _db->execSqlSync(
            "INSERT OR IGNORE INTO schema_migrations(version) VALUES(?);",
            version);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "[SQLiteInitializer] Failed to record migration v"
                  << version << ": " << e.what();
    }
}

void SQLiteInitializer::Exec(const std::string& sql)
{
    try
    {
        _db->execSqlSync(sql);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "[SQLiteInitializer] SQL error: " << e.what()
                  << "\n  SQL: " << sql;
    }
}
