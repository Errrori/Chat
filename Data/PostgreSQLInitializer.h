#pragma once
#include <drogon/orm/DbClient.h>
#include "IDbInitializer.h"

/**
 * @brief PostgreSQL-specific schema initializer.
 *
 * Drop-in replacement for SQLiteInitializer when the project migrates to
 * PostgreSQL. The interface is identical; only the DDL dialect changes:
 *
 *   SQLite  → PostgreSQL
 *   -------   ----------
 *   INTEGER PRIMARY KEY AUTOINCREMENT  →  SERIAL PRIMARY KEY / BIGSERIAL
 *   TEXT                               →  TEXT / VARCHAR
 *   strftime('%s','now')               →  EXTRACT(EPOCH FROM now())::BIGINT
 *   PRAGMA …                           →  SET … (session-level settings)
 *   CREATE INDEX IF NOT EXISTS         →  CREATE INDEX IF NOT EXISTS  (same)
 *
 * To activate, replace in Container.cpp:
 *   _db_initializer = std::make_shared<SQLiteInitializer>(db);
 * with:
 *   _db_initializer = std::make_shared<PostgreSQLInitializer>(db);
 *
 * Make sure the Drogon db_clients entry in config.json uses rdbms="postgresql".
 */
class PostgreSQLInitializer : public IDbInitializer
{
public:
    explicit PostgreSQLInitializer(drogon::orm::DbClientPtr db);

    /// Create all tables + indexes (idempotent, uses IF NOT EXISTS).
    void Initialize() override;

    /// Apply pending versioned migrations and update schema_migrations table.
    void Migrate() override;

    /**
     * @brief Print all pending migrations to LOG_INFO without executing them.
     *
     * Use this in production workflows where DDL is applied by a DBA or CI
     * pipeline before deploying a new server binary.
     * After the DBA executes the printed SQL, they must also run:
     *   INSERT INTO schema_migrations(version) VALUES(<N>);
     * for each applied migration.
     */
    void DumpPendingMigrations() const;

private:
    void EnsureMigrationTable();
    int  GetCurrentVersion() const;
    void SetVersion(int version);
    void Exec(const std::string& sql);

    drogon::orm::DbClientPtr _db;
};
