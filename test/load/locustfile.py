"""
ChatServer HTTP 压力测试
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
包含两个场景，可同时运行也可单独选择：

  AuthUser  (weight=1) — 高频认证路径压测
    · POST /auth/login     — JWT 签发 + 密码验证
    · POST /auth/refresh   — Refresh token 换取新 access token

  ApiUser   (weight=3) — 业务接口混合负载（模拟真实用户行为）
    · GET  /user/profile
    · GET  /thread/record/overview
    · GET  /relation/notifications
    · GET  /user/search
    · GET  /relation/friend-requests
    · GET  /thread/record/user       (需要有效 thread_id)

快速启动:
  # 全场景，带 Web UI（http://localhost:8089）
  locust -f locustfile.py --host http://127.0.0.1:10086

  # 无 UI，中等规模(200并发，20/s 爬坡，跑 5 分钟)
  locust -f locustfile.py --headless --users 200 --spawn-rate 20 \\
         --run-time 5m --host http://127.0.0.1:10086

  # 仅运行 AuthUser
  locust -f locustfile.py --headless --users 200 --spawn-rate 20 \\
         --run-time 5m --host http://127.0.0.1:10086 \\
         -u 200 --class-picker AuthUser

  # 分布式模式（多核）：先启动 master，再启动 N 个 worker
  locust -f locustfile.py --master --host http://127.0.0.1:10086
  locust -f locustfile.py --worker --master-host 127.0.0.1   # 重复启动 N 个
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

from locust import task, between
from locust.contrib.fasthttp import FastHttpUser
from user_pool import checkout, PASSWORD


# ═══════════════════════════════════════════════════════════════════
#  公共基类
# ═══════════════════════════════════════════════════════════════════

class _ChatBase(FastHttpUser):
    """为所有 ChatServer HTTP 场景提供账号分配、登录、token 自动刷新"""
    abstract = True

    # ── 生命周期 ────────────────────────────────────────────────────

    def on_start(self):
        self._account, self._password = checkout()
        # 注册（幂等）：已注册则跳过，不计入失败统计
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
            resp.success()  # setup 阶段不论结果都标记成功
        self._login()

    # ── 认证工具 ────────────────────────────────────────────────────

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
                self._uid = data.get("uid", "")
                resp.success()
            else:
                self._token = ""
                self._uid = ""
                resp.failure(f"login failed: HTTP {resp.status_code}")

    def _headers(self) -> dict:
        return {"Authorization": f"Bearer {self._token}"}

    # ── 带 token 刷新的 HTTP 工具 ──────────────────────────────────

    def _get(self, path: str, params: dict = None):
        """GET 请求；遇 401 自动重新登录并标记失败（下次重试正常）。"""
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

    def _post(self, path: str, body: dict = None):
        """POST 请求；遇 401 自动重新登录并标记失败。"""
        with self.client.post(
            path,
            json=body or {},
            name=path,
            headers=self._headers(),
            catch_response=True,
        ) as resp:
            if resp.status_code == 401:
                self._login()
                resp.failure("token expired, re-authenticated")
            elif resp.status_code >= 400:
                resp.failure(f"HTTP {resp.status_code}")
            else:
                resp.success()


# ═══════════════════════════════════════════════════════════════════
#  场景 A — 认证路径压测
# ═══════════════════════════════════════════════════════════════════

class AuthUser(_ChatBase):
    """
    高频触发 /auth/login 和 /auth/refresh，专门测试：
      · JWT 签发 CPU 开销
      · 密码哈希（MD5）验证延迟
      · Redis refresh token 读写性能
    """
    weight = 1
    wait_time = between(0.2, 1.0)

    @task(3)
    def do_login(self):
        """反复登录：每次签发新 JWT，压测签发吞吐量"""
        with self.client.post(
            "/auth/login",
            json={"account": self._account, "password": self._password},
            name="/auth/login",
            catch_response=True,
        ) as resp:
            if resp.status_code == 200:
                data = resp.json().get("data", {})
                # 更新 token，使 refresh 任务能正常工作
                self._token = data.get("token", self._token)
                resp.success()
            else:
                resp.failure(f"HTTP {resp.status_code}")

    @task(2)
    def do_refresh(self):
        """
        使用 refresh token（cookie）换取新 access token。
        HttpUser 的 Session 会自动携带登录时服务端写入的 cookie。
        """
        with self.client.post(
            "/auth/refresh",
            name="/auth/refresh",
            catch_response=True,
        ) as resp:
            if resp.status_code == 200:
                data = resp.json().get("data", {})
                new_token = data.get("token", "")
                if new_token:
                    self._token = new_token
                resp.success()
            elif resp.status_code == 401:
                # refresh token 过期，重新全量登录
                self._login()
                resp.success()
            else:
                resp.failure(f"HTTP {resp.status_code}")


# ═══════════════════════════════════════════════════════════════════
#  场景 B — 业务接口混合负载
# ═══════════════════════════════════════════════════════════════════

class ApiUser(_ChatBase):
    """
    模拟真实用户的混合读写行为，主要激活数据库查询。
    任务权重体现真实流量分布：读接口频率远高于写接口。
    """
    weight = 3
    wait_time = between(0.3, 1.5)

    def on_start(self):
        super().on_start()
        self._thread_id: int = 0
        self._setup_group_thread()

    def _setup_group_thread(self):
        """预创建一个群聊用于消息记录相关接口的测试"""
        with self.client.post(
            "/thread/create/group-chat",
            json={"name": f"load_{self._account}", "description": "load test group"},
            name="/thread/create/group-chat [setup]",
            headers=self._headers(),
            catch_response=True,
        ) as resp:
            if resp.status_code == 200:
                data = resp.json().get("data", {})
                self._thread_id = data.get("thread_id", 0)
                resp.success()
            else:
                resp.failure(f"group thread setup: HTTP {resp.status_code}")

    # ── 读类接口（高频）─────────────────────────────────────────────

    @task(6)
    def get_profile(self):
        """用户个人信息读取（Redis + DB）"""
        self._get("/user/profile", params={"uid": self._uid})

    @task(5)
    def get_overview(self):
        """会话列表概览——涉及多表 JOIN，是最重的 DB 查询之一"""
        self._get("/thread/record/overview", params={"existing_id": 0})

    @task(4)
    def get_notifications(self):
        """通知列表（分页）"""
        self._get("/relation/notifications", params={"offset": 0, "limit": 20})

    @task(3)
    def search_user(self):
        """按账号搜索用户——触发 users 表 WHERE account = ? 查询"""
        self._get("/user/search", params={"account": self._account})

    @task(2)
    def get_friend_requests(self):
        """待处理好友申请列表"""
        self._get("/relation/friend-requests")

    # ── 读类接口（中频）─────────────────────────────────────────────

    @task(2)
    def get_thread_records(self):
        """拉取聊天记录（需要有效的 thread_id）"""
        if self._thread_id:
            self._get("/thread/record/user", params={"thread_id": self._thread_id})

    @task(1)
    def get_unread_notifications(self):
        """未读通知计数（Redis 缓存优先）"""
        self._get("/relation/notifications/unread")
