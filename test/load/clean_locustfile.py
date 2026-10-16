"""
ChatServer HTTP 压力测试（干净版）
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
与 locustfile.py 场景、权重、路由完全一致，唯一区别：

  · on_start 登录失败会重试，确保每个用户带着有效 token 进入压测；
  · 业务请求遇到 401（token 过期 / 建连尖峰期间登录未就绪）时，
    客户端自动重新登录并重试一次，统计只计最终结果。

这样报告的 Failure 列表只保留真正的服务端错误，不再被
"token expired, re-authenticated" 这类自愈型噪声刷屏。

快速启动:
  locust -f clean_locustfile.py --headless --users 1000 --spawn-rate 50 \\
         --run-time 3m --host http://127.0.0.1:10086 \\
         --html report.html --csv results
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))

from locust import task, between
from locust.contrib.fasthttp import FastHttpUser
from user_pool import checkout, PASSWORD


class _ChatBase(FastHttpUser):
    """为所有 ChatServer HTTP 场景提供账号分配、登录、token 自动刷新"""
    abstract = True

    # ── 生命周期 ────────────────────────────────────────────────────

    def on_start(self):
        self._account, self._password = checkout()
        self._token = ""
        self._uid = ""
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
        self._login_until_ok()

    # ── 认证工具 ───────────────────────────────────────────────────

    def _login(self) -> bool:
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
                if self._token:
                    resp.success()
                    return True
                resp.failure("login response missing token")
                return False
            resp.failure(f"login failed: HTTP {resp.status_code}")
            return False

    def _login_until_ok(self, attempts: int = 5) -> bool:
        """登录重试，确保进入压测时 token 一定有效"""
        for i in range(attempts):
            if self._login():
                return True
            time.sleep(0.2 * (i + 1))
        return False

    def _headers(self) -> dict:
        return {"Authorization": f"Bearer {self._token}"}

    # ── 带 token 刷新的 HTTP 工具（401 自愈 + 重试）─────────────────

    def _get(self, path: str, params: dict = None):
        for attempt in range(2):
            kwargs = {"name": path, "headers": self._headers(), "catch_response": True}
            if params:
                kwargs["params"] = params

            with self.client.get(path, **kwargs) as resp:
                if resp.status_code == 401 and attempt == 0:
                    resp.success()          # 抑制本次仅由 token 过期产生的噪声
                    self._login_until_ok()  # 重新登录后重试
                    continue
                if resp.status_code == 401:
                    resp.failure("unauthorized after re-login")
                elif resp.status_code >= 400:
                    resp.failure(f"HTTP {resp.status_code}")
                else:
                    resp.success()

    def _post(self, path: str, body: dict = None):
        for attempt in range(2):
            with self.client.post(
                path,
                json=body or {},
                name=path,
                headers=self._headers(),
                catch_response=True,
            ) as resp:
                if resp.status_code == 401 and attempt == 0:
                    resp.success()
                    self._login_until_ok()
                    continue
                if resp.status_code == 401:
                    resp.failure("unauthorized after re-login")
                elif resp.status_code >= 400:
                    resp.failure(f"HTTP {resp.status_code}")
                else:
                    resp.success()


# ═══════════════════════════════════════════════════════════════════
#  场景 A — 认证路径压测
# ═══════════════════════════════════════════════════════════════════

class AuthUser(_ChatBase):
    weight = 1
    wait_time = between(0.2, 1.0)

    @task(3)
    def do_login(self):
        with self.client.post(
            "/auth/login",
            json={"account": self._account, "password": self._password},
            name="/auth/login",
            catch_response=True,
        ) as resp:
            if resp.status_code == 200:
                data = resp.json().get("data", {})
                self._token = data.get("token", self._token)
                resp.success()
            else:
                resp.failure(f"HTTP {resp.status_code}")

    @task(2)
    def do_refresh(self):
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
                # 轮换导致 cookie 中的 refresh token 失效：重新全量登录，不算失败
                self._login_until_ok()
                resp.success()
            else:
                resp.failure(f"HTTP {resp.status_code}")


# ═══════════════════════════════════════════════════════════════════
#  场景 B — 业务接口混合负载
# ═══════════════════════════════════════════════════════════════════

class ApiUser(_ChatBase):
    weight = 3
    wait_time = between(0.3, 1.5)

    def on_start(self):
        super().on_start()
        self._thread_id: int = 0
        self._setup_group_thread()

    def _setup_group_thread(self):
        for attempt in range(2):
            with self.client.post(
                "/thread/create/group-chat",
                json={"name": f"load_{self._account}", "description": "load test group"},
                name="/thread/create/group-chat [setup]",
                headers=self._headers(),
                catch_response=True,
            ) as resp:
                if resp.status_code == 200:
                    self._thread_id = resp.json().get("data", {}).get("thread_id", 0)
                    resp.success()
                    return
                if resp.status_code == 401 and attempt == 0:
                    resp.success()
                    self._login_until_ok()
                    continue
                resp.failure(f"group thread setup: HTTP {resp.status_code}")

    @task(6)
    def get_profile(self):
        self._get("/user/profile", params={"uid": self._uid})

    @task(5)
    def get_overview(self):
        self._get("/thread/record/overview", params={"existing_id": 0})

    @task(4)
    def get_notifications(self):
        self._get("/relation/notifications", params={"offset": 0, "limit": 20})

    @task(3)
    def search_user(self):
        self._get("/user/search", params={"account": self._account})

    @task(2)
    def get_friend_requests(self):
        self._get("/relation/friend-requests")

    @task(2)
    def get_thread_records(self):
        if self._thread_id:
            self._get("/thread/record/user", params={"thread_id": self._thread_id})

    @task(1)
    def get_unread_notifications(self):
        self._get("/relation/notifications/unread")
