-- 仅在 PostgreSQL 数据卷首次初始化时执行（/docker-entrypoint-initdb.d）。
-- 配合 docker-compose.yml 中 postgres 的
--   command: postgres -c shared_preload_libraries=pg_stat_statements
-- 使用，使压测后可直接查询 pg_stat_statements 定位慢查询。
CREATE EXTENSION IF NOT EXISTS pg_stat_statements;
