# ChatServer 压力测试指南

测试套件基于 [Locust](https://locust.io/) 框架，覆盖 HTTP 接口吞吐量、WebSocket 并发连接、JWT 认证性能和数据库查询瓶颈四个维度。

## 目录结构

```
test/load/
├── requirements.txt    依赖声明
├── user_pool.py        测试账号池（环境变量可配置）
├── locustfile.py       HTTP 场景（AuthUser + ApiUser）
├── ws_locustfile.py    WebSocket 并发场景
└── README.md           本文件
```

---

## 安装

```bash
cd ChatServer/test/load
pip install -r requirements.txt
```

---

## 预热（首次运行必做）

压测开始前先批量预注册所有测试账号，避免 Locust 启动时大量注册请求干扰延迟指标：

```bash
# 默认注册 500 个账号（30 并发线程）
python warm_up.py --host http://127.0.0.1:10086

# 大规模：预注册 1000 个账号，50 并发线程
python warm_up.py --host http://127.0.0.1:10086 --size 1000 --workers 50

# 跳过登录验证（更快，仅注册）
python warm_up.py --host http://127.0.0.1:10086 --no-verify
```

预热成功输出示例：

```
✔ 注册成功:  500 / 500
✔ 登录验证:  500 / 500
  完成，耗时 8.4s，平均速率 59.5 账号/s
```

若有失败账号，详情会写入 `failed_accounts.txt`，脚本以非零退出码退出（可用于 CI 流水线卡门）。

---

## 快速开始

> 启动前请确保服务器、PostgreSQL 和 Redis 均已就绪，并已完成上面的预热步骤。

### 带 Web UI（推荐初次使用）

```bash
locust -f locustfile.py --host http://127.0.0.1:10086
```

浏览器打开 `http://localhost:8089`，填写并发用户数和爬坡速率后点击 **Start** 即可看到实时 RPS、延迟曲线和错误率。

### 无头模式（适合 CI / 自动化）

```bash
# 中等规模：200 并发，20/s 爬坡，持续 5 分钟
locust -f locustfile.py --headless \
       --users 200 --spawn-rate 20 --run-time 5m \
       --host http://127.0.0.1:10086

# 大规模：1000 并发，50/s 爬坡，持续 10 分钟
locust -f locustfile.py --headless \
       --users 1000 --spawn-rate 50 --run-time 10m \
       --host http://127.0.0.1:10086

# 仅测认证路径（AuthUser）
locust -f locustfile.py --headless \
       --users 300 --spawn-rate 30 --run-time 5m \
       --host http://127.0.0.1:10086 \
       --class-picker          # 先弹出类选择 UI，选 AuthUser
```

### WebSocket 并发测试

```bash
# 500 个持久连接，10/s 爬坡
locust -f ws_locustfile.py --headless \
       --users 500 --spawn-rate 10 --run-time 10m \
       --host http://127.0.0.1:10086

# 1000 个持久连接（大规模，建议分布式模式，见下文）
locust -f ws_locustfile.py --headless \
       --users 1000 --spawn-rate 20 --run-time 10m \
       --host http://127.0.0.1:10086
```

---

## 分布式模式（1000+ 并发）

当单机无法驱动足够多的用户时，使用 master/worker 架构横向扩展。

```bash
# 1. 在主控机启动 master（不产生用户，只聚合统计）
locust -f locustfile.py --master --host http://127.0.0.1:10086

# 2. 在各 worker 机启动 worker（实际产生并发请求）
#    可启动多个 worker，--expect-workers 告诉 master 等待几个 worker 就绪
locust -f locustfile.py --worker --master-host <master_ip>

# 3. 无头分布式（master 等到 3 个 worker 后自动开始）
locust -f locustfile.py --master --headless \
       --users 3000 --spawn-rate 100 --run-time 10m \
       --expect-workers 3 --host http://127.0.0.1:10086
```

---

## 环境变量

可在运行前设置以下环境变量调整账号池行为：

| 变量名 | 默认值 | 说明 |
|---|---|---|
| `LOAD_TEST_POOL_SIZE` | `500` | 账号池大小（并发用户 > 此值时账号循环复用） |
| `LOAD_TEST_ACCOUNT_PREFIX` | `lt_user_` | 账号名前缀 |
| `LOAD_TEST_PASSWORD` | `LoadTest@123` | 所有测试账号使用相同密码 |

```bash
# 示例：使用 1000 个独立账号
set LOAD_TEST_POOL_SIZE=1000   # Windows
export LOAD_TEST_POOL_SIZE=1000  # Linux/macOS
```

---

## 测试场景说明

### HTTP 场景（locustfile.py）

| 用户类 | 权重 | Wait Time | 测试目标 |
|---|---|---|---|
| `AuthUser` | 1 | 0.2~1.0s | JWT 签发速度、密码验证延迟、Redis refresh token 读写 |
| `ApiUser` | 3 | 0.3~1.5s | 多表 JOIN 查询、Redis 缓存命中率、接口整体吞吐 |

**ApiUser 任务权重分布：**

| 接口 | 权重 | 说明 |
|---|---|---|
| `GET /user/profile` | 6 | 最高频，Redis 缓存优先 |
| `GET /thread/record/overview` | 5 | 最重 DB 查询，多表 JOIN |
| `GET /relation/notifications` | 4 | 分页查询 |
| `GET /user/search` | 3 | 按账号搜索 |
| `GET /relation/friend-requests` | 2 | 好友申请列表 |
| `GET /thread/record/user` | 2 | 聊天记录 |
| `GET /relation/notifications/unread` | 1 | 未读计数 |

### WebSocket 场景（ws_locustfile.py）

每个 Locust User 在启动时：
1. 注册账号（首次）→ 登录获取 JWT token
2. 创建一个群聊（获取 `thread_id`）
3. 建立 WebSocket 长连接到 `/ws/chat?token=xxx`
4. 每隔 3~8 秒发送一条消息

上报的 Locust 指标：

| 指标名 | 类型 | 含义 |
|---|---|---|
| `ws_connect` | WS | 连接建立耗时（含 HTTP Upgrade 握手） |
| `ws_send_message` | WS | 消息发送耗时（write 系统调用层面） |
| `ws_setup_login` | HTTP | 登录 HTTP 接口耗时 |

---

## 关键指标解读

| 指标 | 健康阈值 | 警告阈值 | 说明 |
|---|---|---|---|
| RPS（每秒请求数） | 尽量高 | — | 服务器整体吞吐能力 |
| P50 延迟 | < 100ms | > 300ms | 中位数延迟 |
| P95 延迟 | < 500ms | > 1s | 长尾延迟，影响用户体验 |
| P99 延迟 | < 1s | > 3s | 极端慢请求比例 |
| 失败率 | < 1% | > 5% | 接口错误率 |
| `ws_connect` P95 | < 200ms | > 1s | WS 连接建立速度 |

---

## 数据库慢查询分析

压测时同步开启 PostgreSQL 慢查询日志，可精确定位瓶颈查询：

### 1. 启用 pg_stat_statements

```sql
-- 在 PostgreSQL 执行
CREATE EXTENSION IF NOT EXISTS pg_stat_statements;

-- 重置统计（压测前执行）
SELECT pg_stat_statements_reset();
```

### 2. 压测结束后查看 Top-10 慢查询

```sql
SELECT
    calls,
    round(mean_exec_time::numeric, 2)  AS avg_ms,
    round(total_exec_time::numeric, 2) AS total_ms,
    round((stddev_exec_time)::numeric, 2) AS stddev_ms,
    rows,
    left(query, 120) AS query_snippet
FROM pg_stat_statements
ORDER BY total_exec_time DESC
LIMIT 10;
```

### 3. 常见瓶颈表现与排查

| 现象 | 可能原因 | 排查方向 |
|---|---|---|
| `GET /thread/record/overview` P95 > 500ms | messages 表全表扫描 | 检查 `thread_id` + `created_at` 复合索引 |
| `GET /relation/notifications` 延迟高 | notifications 表缺少 `user_id` 索引 | 添加 `CREATE INDEX` |
| `GET /user/search` 慢 | account 字段无索引 | 添加 UNIQUE INDEX |
| 连接池耗尽（DrogonDB timeout） | `connection_number` 配置过低 | 增大 config.json 中的 `connection_number` |
| Redis 延迟突增 | Redis 单线程阻塞 | 检查大 key，避免 KEYS * 操作 |

---

## 生成测试报告

```bash
# 生成 HTML 报告（--html）和 CSV 数据（--csv）
locust -f locustfile.py --headless \
       --users 200 --spawn-rate 20 --run-time 5m \
       --host http://127.0.0.1:10086 \
       --html report.html \
       --csv results
```

生成文件：
- `report.html` — 可在浏览器中查看的完整报告（含图表）
- `results_stats.csv` — 每个接口的汇总统计
- `results_stats_history.csv` — 按时间序列的 RPS/延迟数据
- `results_failures.csv` — 失败请求详情

---

## 常见问题

**Q: 大量 `token expired` 失败怎么办？**  
A: 增加服务端 access token 有效期，或降低 `AuthUser` 的 wait_time 下限减少时间间隔。

**Q: WebSocket 连接数上不去，停在某个数字？**  
A: 检查操作系统的文件描述符限制（Linux: `ulimit -n`），以及 Drogon 的 `threads_num` 配置。

**Q: `ws_send_message` 全部失败？**  
A: 确认服务器已运行，且 `/ws/chat` 路径正确。检查 token 是否过期（WS 连接建立后 token 若到期服务端会主动断连）。

**Q: 如何只测某一个 User 类？**  
```bash
# 使用 --class-picker 在 Web UI 中选择，或直接在命令行指定
locust -f locustfile.py --headless -u 100 -r 10 --run-time 3m \
       --host http://127.0.0.1:10086 ApiUser
```
