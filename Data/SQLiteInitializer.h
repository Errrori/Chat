#pragma once
#include <drogon/orm/DbClient.h>
#include "IDbInitializer.h"

/**
 * @brief SQLite-specific schema initializer.
 *
 * Owns all SQLite DDL (CREATE TABLE, PRAGMA, indexes) and the migration
 * version table. Replaces the db_table_list in const.h and the inline loop
 * in Container::Container().
 *
 * To add a new table or alter an existing one, append a new Migration entry
 * to MIGRATIONS inside SQLiteInitializer.cpp – do NOT touch const.h.
 */
class SQLiteInitializer : public IDbInitializer
{
public:
    explicit SQLiteInitializer(drogon::orm::DbClientPtr db);

    /// Create all tables + indexes (idempotent, uses IF NOT EXISTS).
    void Initialize() override;

    /// Apply pending versioned migrations and update schema_migrations table.
    void Migrate() override;

private:
    /// Create the migration-tracking table if it doesn't exist yet.
    void EnsureMigrationTable();

    /// Return the highest migration version already applied (0 if none).
    int  GetCurrentVersion();

    /// Record that a migration version was successfully applied.
    void SetVersion(int version);

    /// Execute a single SQL statement, logging errors but not throwing.
    void Exec(const std::string& sql);

    drogon::orm::DbClientPtr _db;
};
