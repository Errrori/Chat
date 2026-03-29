"""
ChatServer 性能基准压测
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
专注测试以下六条路由，用于「添加索引前 vs 后」以及「Redis 缓存关闭 vs 开启」
的前后对比实验：

  GET /user/profile                  — users.uid 查询（可验证 Redis 缓存效果）
  GET /user/search                   — users.account 查询
  GET /relation/notifications        — notifications.recipient_uid 查询
  GET /relation/notifications/unread — notifications.recipient_uid + is_read 查询
  GET /relation/friend-requests      — friend_requests.requester_uid / target_uid 查询
  GET /thread/record/overview        — messages CTE + private_chats / group_members 查询

快速开始:
  # ① 基准（无索引 / 无缓存）
  locust -f benchmark_locustfile.py --headless \\
         --users 200 --spawn-rate 20 --run-time 3m \\
         --host http://127.0.0.1:10086 \\
         --html baseline.html --csv baseline

  # ② 优化后（已加索引 / 已启用 Redis）
  locust -f benchmark_locustfile.py --headless \\
         --users 200 --spawn-rate 20 --run-time 3m \\
         --host http://127.0.0.1:10086 \\
         --html optimized.html --csv optimized

  # 带 Web UI（适合调试阶段观察实时曲线）
  locust -f benchmark_locustfile.py --host http://127.0.0.1:10086

环境变量（与 user_pool.py 共享）:
  LOAD_TEST_POOL_SIZE       账号池大小    (default: 500)
  LOAD_TEST_ACCOUNT_PREFIX  账号名前缀    (default: ltuser)
  LOAD_TEST_PASSWORD        统一密码      (default: LoadTest@123)
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

from locust import task, between, events
from locust.contrib.fasthttp import FastHttpUser
from user_pool import checkout, PASSWORD


# ═══════════════════════════════════════════════════════════════════
#  基类：账号分配 + 登录 + token 自动刷新
# ═══════════════════════════════════════════════════════════════════

class _BenchBase(FastHttpUser):
    abstract = True

    def on_start(self):
        self._account, self._password = checkout()
        # 注册（幂等，已存在返回 409 也视为成功）
        with self.client.post(
            "/auth/register",
            json={
                "account": self._account,
                "password": self._password,
                "username": self._account,
            },
            name="/auth/register [setup]",
            catch_response=True,
        ) as resp:
            resp.success()

        self._login()

    def _login(self):
        with self.client.post(
            "/auth/login",
            json={"account": self._account, "password": self._password},
            name="/auth/login [setup]",
            catch_response=True,
        ) as resp:
            if resp.status_code == 200:
                data = resp.json().get("data", {})
                self._token = data.get("token", "")
                self._uid   = data.get("uid", "")
                resp.success()
            else:
                self._token = ""
                self._uid   = ""
                resp.failure(f"login failed: HTTP {resp.status_code}")

    def _headers(self) -> dict:
        return {"Authorization": f"Bearer {self._token}"}

    def _get(self, path: str, params: dict = None):
        kwargs = {"name": path, "headers": self._headers(), "catch_response": True}
        if params:
            kwargs["params"] = params
        with self.client.get(path, **kwargs) as resp:
            if resp.status_code == 401:
                self._login()
                resp.failure("token expired, re-authenticated")
            elif resp.status_code >= 400:
                resp.failure(f"HTTP {resp.status_code}")
            else:
                resp.success()


# ═══════════════════════════════════════════════════════════════════
#  DBBenchUser — 专门测试数据库索引影响的六条路由
#
#  任务权重设计依据：
#    /user/profile       权重 6  最高频，同时验证 Redis 缓存
#    /thread/record/overview 权重 5  CTE 多表查询，最重
#    /relation/notifications 权重 4  notifications 表全扫
#    /user/search        权重 3  users.account 查询
#    /relation/friend-requests 权重 2  friend_requests 双向查询
#    /relation/notifications/unread 权重 2  复合条件查询
# ═══════════════════════════════════════════════════════════════════

class DBBenchUser(_BenchBase):
    """
    数据库 / 缓存性能基准用户。

    分两轮运行：
      第一轮 — 无索引 + 无 Redis 缓存（baseline）
      第二轮 — 添加索引 + 启用 Redis 缓存（optimized）

    关注指标：每条路由的 Median、P95、P99 和 RPS。
    """
    weight = 1
    wait_time = between(0.2, 0.8)  # 比 locustfile.py 更短，制造更高并发压力

    def on_start(self):
        super().on_start()
        # 预创建一个群聊，用于 overview 接口
        self._thread_id: int = 0
        with self.client.post(
            "/thread/create/group-chat",
            json={"name": f"bench_{self._account}", "description": "benchmark group"},
            name="/thread/create/group-chat [setup]",
            headers=self._headers(),
            catch_response=True,
        ) as resp:
            if resp.status_code == 200:
                data = resp.json().get("data", {})
                self._thread_id = data.get("thread_id", 0)
                resp.success()
            else:
                resp.success()  # setup 失败不影响其他接口测试

    # ── 六条被测路由 ─────────────────────────────────────────────

    @task(6)
    def bench_user_profile(self):
        """
        GET /user/profile
        优化前：每次直接查 users 表（WHERE uid = ?）
        优化后：先查 Redis HGETALL，命中则跳过 DB（理论延迟 <1ms vs ~5ms）
        """
        self._get("/user/profile", params={"uid": self._uid})

    @task(5)
    def bench_thread_overview(self):
        """
        GET /thread/record/overview
        核心 CTE 查询 messages(thread_id IN (...) AND message_id > ?)
        已有 idx_messages_thread_msg 索引；
        但 private_chats(uid1 OR uid2) 查询无法走单列索引，
        此处主要观察数据量增大后的变化趋势。
        """
        self._get("/thread/record/overview", params={"existing_id": 0})

    @task(4)
    def bench_notifications(self):
        """
        GET /relation/notifications
        优化前：全表扫描 notifications WHERE recipient_uid = ?
        优化后：走 idx_notifications_recipient(recipient_uid, is_read)
        数据越多差距越显著。
        """
        self._get("/relation/notifications", params={"offset": 0, "limit": 20})

    @task(3)
    def bench_user_search(self):
        """
        GET /user/search
        users.account 有 UNIQUE 约束（等价于 B-tree 索引），本身较快；
        对比值：验证在高并发下 DB 连接池是否成为瓶颈。
        """
        self._get("/user/search", params={"account": self._account})

    @task(2)
    def bench_friend_requests(self):
        """
        GET /relation/friend-requests
        优化前：扫描 friend_requests WHERE requester_uid = ? OR target_uid = ?
        优化后：走 idx_friend_requests_requester + idx_friend_requests_target
        """
        self._get("/relation/friend-requests")

    @task(2)
    def bench_unread_notifications(self):
        """
        GET /relation/notifications/unread
        优化前：扫描 notifications WHERE recipient_uid = ? AND is_read = 0
        优化后：复合索引 idx_notifications_recipient(recipient_uid, is_read) 覆盖
        """
        self._get("/relation/notifications/unread")
