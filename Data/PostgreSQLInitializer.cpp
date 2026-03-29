#include "pch.h"
#include "PostgreSQLInitializer.h"

// ---------------------------------------------------------------------------
// Versioned migration registry (PostgreSQL dialect)
//
// Key differences from SQLite:
//   - SERIAL / BIGSERIAL  instead of INTEGER AUTOINCREMENT
//   - EXTRACT(EPOCH FROM now())::BIGINT  instead of strftime('%s','now')
//   - No PRAGMA statements (use SET for session-level config if needed)
//   - TEXT and VARCHAR are interchangeable; VARCHAR(n) preferred for fixed-width
//
// HOW TO ADD A SCHEMA CHANGE:
//   1. Append a new Migration entry at the END of this list.
//   2. Give it the next sequential version number.
//   3. Write the PostgreSQL SQL for the change.
//   4. Never modify or reorder existing entries.
// ---------------------------------------------------------------------------

namespace {

struct Migration
{
    int         version;
    std::string description;
    std::string sql;
};

const std::vector<Migration> MIGRATIONS = {
    // --- v1: base schema -------------------------------------------------------
    {
        1, "Create threads table",
        "CREATE TABLE IF NOT EXISTS threads ("
        "  thread_id   BIGSERIAL    PRIMARY KEY,"
        "  type        INTEGER      NOT NULL,"    // 0=group, 1=private, 2=ai
        "  create_time BIGINT       NOT NULL DEFAULT (EXTRACT(EPOCH FROM now())::BIGINT)"
        ");"
    },
    {
        2, "Create users table",
        "CREATE TABLE IF NOT EXISTS users ("
        "  id          BIGSERIAL    PRIMARY KEY,"
        "  username    TEXT         NOT NULL,"
        "  account     TEXT         NOT NULL UNIQUE,"
        "  password    TEXT         NOT NULL,"
        "  uid         TEXT         NOT NULL UNIQUE,"
        "  avatar      TEXT         DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
        "  create_time BIGINT       NOT NULL DEFAULT (EXTRACT(EPOCH FROM now())::BIGINT),"
        "  signature   TEXT,"
        "  email       TEXT"
        ");"
    },
    {
        3, "Create ai_chats table",
        "CREATE TABLE IF NOT EXISTS ai_chats ("
        "  thread_id     BIGINT NOT NULL REFERENCES threads(thread_id) ON DELETE CASCADE,"
        "  name          TEXT   NOT NULL,"
        "  creator_uid   TEXT   REFERENCES users(uid) ON DELETE CASCADE,"
        "  avatar        TEXT   DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
        "  init_settings TEXT"
        ");"
    },
    {
        4, "Create private_chats table",
        "CREATE TABLE IF NOT EXISTS private_chats ("
        "  thread_id BIGINT NOT NULL REFERENCES threads(thread_id) ON DELETE CASCADE,"
        "  uid1      TEXT   NOT NULL REFERENCES users(uid)         ON DELETE CASCADE,"
        "  uid2      TEXT   NOT NULL REFERENCES users(uid)         ON DELETE CASCADE,"
        "  UNIQUE (uid1, uid2)"
        ");"
    },
    {
        5, "Create group_chats table",
        "CREATE TABLE IF NOT EXISTS group_chats ("
        "  thread_id   BIGINT NOT NULL REFERENCES threads(thread_id) ON DELETE CASCADE,"
        "  name        TEXT   NOT NULL,"
        "  avatar      TEXT   DEFAULT 'https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png',"
        "  description TEXT"
        ");"
    },
    {
        6, "Create group_members table",
        "CREATE TABLE IF NOT EXISTS group_members ("
        "  thread_id BIGINT  NOT NULL REFERENCES threads(thread_id) ON DELETE CASCADE,"
        "  user_uid  TEXT    NOT NULL REFERENCES users(uid)         ON DELETE CASCADE,"
        "  role      INTEGER NOT NULL DEFAULT 0,"   // 0=member, 1=admin, 2=owner
        "  join_time BIGINT  NOT NULL DEFAULT (EXTRACT(EPOCH FROM now())::BIGINT),"
        "  PRIMARY KEY (thread_id, user_uid)"
        ");"
    },
    {
        7, "Create messages table",
        "CREATE TABLE IF NOT EXISTS messages ("
        "  message_id    BIGSERIAL PRIMARY KEY,"
        "  thread_id     BIGINT    NOT NULL REFERENCES threads(thread_id) ON DELETE CASCADE,"
        "  sender_uid    TEXT      NOT NULL REFERENCES users(uid)         ON DELETE CASCADE,"
        "  sender_name   TEXT      NOT NULL,"
        "  sender_avatar TEXT      NOT NULL,"
        "  content       TEXT,"
        "  attachment    TEXT,"                    // JSON attachment descriptor
        "  status        INTEGER   NOT NULL DEFAULT 1,"
        "  create_time   BIGINT    NOT NULL DEFAULT (EXTRACT(EPOCH FROM now())::BIGINT),"
        "  update_time   BIGINT    NOT NULL DEFAULT (EXTRACT(EPOCH FROM now())::BIGINT)"
        ");"
    },
    {
        8, "Create ai_context table",
        "CREATE TABLE IF NOT EXISTS ai_context ("
        "  message_id        TEXT   PRIMARY KEY,"
        "  thread_id         BIGINT NOT NULL,"
        "  role              TEXT   NOT NULL,"
        "  content           TEXT   NOT NULL,"
        "  attachment        TEXT,"
        "  reasoning_content TEXT,"
        "  created_time      BIGINT NOT NULL DEFAULT (EXTRACT(EPOCH FROM now())::BIGINT)"
        ");"
    },
    {
        9, "Create notifications table",
        "CREATE TABLE IF NOT EXISTS notifications ("
        "  id            BIGSERIAL PRIMARY KEY,"
        "  type          INTEGER   NOT NULL CHECK (type IN (0,1,2,3)),"
        // 0=RequestReceived, 1=RequestAccepted, 2=RequestRejected, 3=BlockUser
        "  sender_uid    TEXT      NOT NULL REFERENCES users(uid) ON DELETE CASCADE,"
        "  recipient_uid TEXT      NOT NULL REFERENCES users(uid) ON DELETE CASCADE,"
        "  payload       TEXT,"
        "  is_read       INTEGER   NOT NULL DEFAULT 0 CHECK (is_read IN (0,1)),"
        "  created_time  BIGINT    NOT NULL DEFAULT (EXTRACT(EPOCH FROM now())::BIGINT)"
        ");"
    },
    {
        10, "Create friendships table",
        "CREATE TABLE IF NOT EXISTS friendships ("
        "  uid1         TEXT   NOT NULL REFERENCES users(uid) ON DELETE CASCADE,"
        "  uid2         TEXT   NOT NULL REFERENCES users(uid) ON DELETE CASCADE,"
        "  created_time BIGINT NOT NULL DEFAULT (EXTRACT(EPOCH FROM now())::BIGINT),"
        "  UNIQUE (uid1, uid2),"
        "  CHECK  (uid1 < uid2)"
        ");"
    },
    {
        11, "Create friend_requests table",
        "CREATE TABLE IF NOT EXISTS friend_requests ("
        "  id            BIGSERIAL PRIMARY KEY,"
        "  requester_uid TEXT      NOT NULL REFERENCES users(uid) ON DELETE CASCADE,"
        "  target_uid    TEXT      NOT NULL REFERENCES users(uid) ON DELETE CASCADE,"
        "  status        INTEGER   NOT NULL DEFAULT 0 CHECK (status IN (0,1,2)),"
        // 0=pending, 1=accepted, 2=refused
        "  created_time  BIGINT    NOT NULL DEFAULT (EXTRACT(EPOCH FROM now())::BIGINT),"
        "  updated_time  BIGINT    NOT NULL DEFAULT (EXTRACT(EPOCH FROM now())::BIGINT),"
        "  payload       TEXT"
        ");"
    },
    {
        12, "Create block table",
        "CREATE TABLE IF NOT EXISTS block ("
        "  operator_uid TEXT   NOT NULL REFERENCES users(uid) ON DELETE CASCADE,"
        "  blocked_uid  TEXT   NOT NULL REFERENCES users(uid) ON DELETE CASCADE,"
        "  created_time BIGINT NOT NULL DEFAULT (EXTRACT(EPOCH FROM now())::BIGINT),"
        "  UNIQUE (operator_uid, blocked_uid)"
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
    {
        17, "Index: notifications(recipient_uid, is_read)",
        // Covers GET /relation/notifications  (recipient_uid = ?)
        // and    GET /relation/notifications/unread  (recipient_uid = ? AND is_read = 0)
        "CREATE INDEX IF NOT EXISTS idx_notifications_recipient "
        "ON notifications(recipient_uid, is_read);"
    },
    {
        18, "Index: friend_requests(requester_uid, status)",
        "CREATE INDEX IF NOT EXISTS idx_friend_requests_requester "
        "ON friend_requests(requester_uid, status);"
    },
    {
        19, "Index: friend_requests(target_uid, status)",
        "CREATE INDEX IF NOT EXISTS idx_friend_requests_target "
        "ON friend_requests(target_uid, status);"
    },

    // ---- Append future migrations BELOW this line ----------------------------
    // Example:
    // { 17, "Add last_seen column to users",
    //   "ALTER TABLE users ADD COLUMN IF NOT EXISTS last_seen BIGINT DEFAULT 0;"
    // },
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// PostgreSQLInitializer implementation
// ---------------------------------------------------------------------------

PostgreSQLInitializer::PostgreSQLInitializer(drogon::orm::DbClientPtr db)
    : _db(std::move(db))
{
}

void PostgreSQLInitializer::Initialize()
{
    // PostgreSQL has no PRAGMA; session settings (if any) go here, e.g.:
    //   Exec("SET TIME ZONE 'UTC';");

    EnsureMigrationTable();
}

void PostgreSQLInitializer::Migrate()
{
    const int currentVersion = GetCurrentVersion();
    LOG_INFO << "[PostgreSQLInitializer] Current schema version: " << currentVersion;

    for (const auto& m : MIGRATIONS)
    {
        if (m.version <= currentVersion)
            continue;

        LOG_INFO << "[PostgreSQLInitializer] Applying migration v" << m.version
                 << ": " << m.description;
        Exec(m.sql);
        SetVersion(m.version);
    }

    LOG_INFO << "[PostgreSQLInitializer] Schema is up to date (version "
             << GetCurrentVersion() << ").";
}

void PostgreSQLInitializer::DumpPendingMigrations() const
{
    // Re-read the current version directly (const method, no caching)
    int currentVersion = 0;
    try
    {
        auto result = _db->execSqlSync(
            "SELECT COALESCE(MAX(version), 0) AS v FROM schema_migrations;");
        if (!result.empty())
            currentVersion = result[0]["v"].as<int>();
    }
    catch (...)
    {
        // schema_migrations may not exist yet; treat as version 0
    }

    LOG_INFO << "[PostgreSQLInitializer] === Pending migrations (current v"
             << currentVersion << ") ===";

    bool anyPending = false;
    for (const auto& m : MIGRATIONS)
    {
        if (m.version <= currentVersion)
            continue;

        anyPending = true;
        LOG_INFO << "-- v" << m.version << ": " << m.description;
        LOG_INFO << m.sql;
        LOG_INFO << "INSERT INTO schema_migrations(version) VALUES(" << m.version << ");";
        LOG_INFO << "";
    }

    if (!anyPending)
        LOG_INFO << "[PostgreSQLInitializer] No pending migrations.";
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void PostgreSQLInitializer::EnsureMigrationTable()
{
    Exec(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "  version    INTEGER PRIMARY KEY,"
        "  applied_at BIGINT  NOT NULL DEFAULT (EXTRACT(EPOCH FROM now())::BIGINT)"
        ");"
    );
}

int PostgreSQLInitializer::GetCurrentVersion() const
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
        LOG_WARN << "[PostgreSQLInitializer] Could not read schema version: " << e.what();
    }
    return 0;
}

void PostgreSQLInitializer::SetVersion(int version)
{
    try
    {
        _db->execSqlSync(
            "INSERT INTO schema_migrations(version) VALUES($1) ON CONFLICT DO NOTHING;",
            version);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "[PostgreSQLInitializer] Failed to record migration v"
                  << version << ": " << e.what();
    }
}

void PostgreSQLInitializer::Exec(const std::string& sql)
{
    try
    {
        _db->execSqlSync(sql);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "[PostgreSQLInitializer] SQL error: " << e.what()
                  << "\n  SQL: " << sql;
    }
}
