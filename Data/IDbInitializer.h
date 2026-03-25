#pragma once

/**
 * @brief Database schema initializer interface.
 *
 * Abstracts schema creation and incremental migration away from the rest of
 * the application. Each concrete implementation encapsulates the DDL dialect
 * of its target database (SQLite, PostgreSQL, …).
 *
 * Usage in Container:
 *   _db_initializer = std::make_shared<SQLiteInitializer>(db);
 *   _db_initializer->Initialize();
 *   _db_initializer->Migrate();
 *
 * To switch databases, replace the concrete type above – nothing else changes.
 */
class IDbInitializer
{
public:
    virtual ~IDbInitializer() = default;

    /**
     * @brief Create all application tables if they do not exist yet.
     *
     * Must be idempotent: safe to call on every startup whether the database
     * is brand-new or already fully initialized.
     */
    virtual void Initialize() = 0;

    /**
     * @brief Apply any pending incremental schema changes (migrations).
     *
     * Implementations should maintain a `schema_migrations` tracking table
     * and execute only the migrations whose version number exceeds the
     * currently recorded version, then update the recorded version.
     *
     * Called after Initialize() on every startup.
     */
    virtual void Migrate() = 0;
};
