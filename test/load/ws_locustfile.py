"""
ChatServer WebSocket 并发压测
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
测试目标:
  · 服务器能承受的最大并发 WebSocket 长连接数
  · 高并发下消息发送的延迟与稳定性
  · 连接建立时延（包含 JWT 验证）

每个 Locust User 对应一个独立的长连接:
  on_start  → 注册 & 登录（HTTP）→ 创建群聊获取 thread_id → 建立 WS
  @task     → 每隔 3~8 秒发送一条消息，上报发送延迟
  on_stop   → 优雅关闭连接

上报的 Locust 指标:
  WS / ws_connect       连接建立耗时（含 HTTP 握手 + WS Upgrade）
  WS / ws_send_message  消息发送耗时（write() 系统调用层面）
  HTTP / ws_setup_login 登录接口耗时（WS User 内部的 HTTP 调用）

快速启动:
  # 500 并发长连接，10/s 爬坡
  locust -f ws_locustfile.py --headless --users 500 --spawn-rate 10 \\
         --run-time 10m --host http://127.0.0.1:10086

  # 1000 并发（大规模，建议配合 master/worker 分布式模式）
  locust -f ws_locustfile.py --master --host http://127.0.0.1:10086
  locust -f ws_locustfile.py --worker --master-host <master_ip>

注意:
  若遇到"too many open files"，需调高系统文件描述符限制:
    Windows: 默认已支持足够多，通常无需调整
    Linux:   ulimit -n 65536
"""

import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))

import requests as req_lib
import websocket
from locust import User, between, events, task

from user_pool import checkout


class WsChatUser(User):
    """
    每个实例维护一条独立的 WebSocket 长连接。
    内部 HTTP 请求（注册、登录、建群）使用 plain requests，
    不经过 Locust 的 HttpUser.client，避免统计数据混淆。
    """

    wait_time = between(3, 8)

    # ── 生命周期 ────────────────────────────────────────────────────

    def on_start(self):
        self._account, self._password = checkout()
        self._token: str = ""
        self._uid: str = ""
        self._thread_id: int = 0
        self._ws: websocket.WebSocket | None = None
        self._base_url: str = self.host  # e.g. http://127.0.0.1:10086

        self._register()
        if self._login():
            self._ensure_thread()
            self._connect_ws()

    def on_stop(self):
        self._close_ws()

    # ── 任务 ────────────────────────────────────────────────────────

    @task
    def send_chat_message(self):
        """
        向预建群聊发送一条消息，测量 write() 调用的延迟。
        若连接已断开则自动尝试重连。
        """
        if not self._ws or not self._thread_id:
            return

        payload = json.dumps({
            "thread_id": self._thread_id,
            "content": f"load test {time.time():.3f}",
        })
        start = time.monotonic()
        try:
            self._ws.send(payload)
            self._fire_ws("ws_send_message", time.monotonic() - start, len(payload))
        except Exception as exc:
            self._fire_ws("ws_send_message", time.monotonic() - start, 0, exc)
            self._ws = None
            # 连接已断，尝试重新登录并重连
            if self._login():
                self._connect_ws()

    # ── 内部: 注册 / 登录 ────────────────────────────────────────────

    def _register(self):
        """注册账号（幂等，忽略 409）"""
        try:
            req_lib.post(
                f"{self._base_url}/auth/register",
                json={
                    "account": self._account,
                    "password": self._password,
                    "username": self._account,
                },
                timeout=10,
            )
        except Exception:
            pass  # 网络超时不阻塞主流程

    def _login(self) -> bool:
        start = time.monotonic()
        try:
            resp = req_lib.post(
                f"{self._base_url}/auth/login",
                json={"account": self._account, "password": self._password},
                timeout=10,
            )
            elapsed = time.monotonic() - start
            if resp.status_code == 200:
                data = resp.json().get("data", {})
                self._token = data.get("token", "")
                self._uid = data.get("uid", "")
                self._fire_http("ws_setup_login", elapsed, len(resp.content))
                return bool(self._token)
            else:
                self._fire_http(
                    "ws_setup_login", elapsed, 0,
                    Exception(f"login HTTP {resp.status_code}"),
                )
                return False
        except Exception as exc:
            self._fire_http("ws_setup_login", time.monotonic() - start, 0, exc)
            return False

    # ── 内部: 群聊准备 ───────────────────────────────────────────────

    def _ensure_thread(self):
        """为当前账号预创建一个群聊，获取 thread_id"""
        try:
            resp = req_lib.post(
                f"{self._base_url}/thread/create/group-chat",
                json={
                    "name": f"ws_load_{self._account}",
                    "description": "ws load test",
                },
                headers={"Authorization": f"Bearer {self._token}"},
                timeout=10,
            )
            if resp.status_code == 200:
                self._thread_id = resp.json().get("data", {}).get("thread_id", 0)
        except Exception:
            pass

    # ── 内部: WebSocket 连接管理 ─────────────────────────────────────

    def _ws_url(self) -> str:
        base = self._base_url.replace("http://", "ws://").replace("https://", "wss://")
        return f"{base}/ws/chat?token={self._token}"

    def _connect_ws(self, retries: int = 3):
        """建立 WebSocket 连接，失败后最多重试 retries 次，上报连接耗时"""
        url = self._ws_url()
        for attempt in range(retries):
            start = time.monotonic()
            try:
                ws = websocket.create_connection(
                    url,
                    timeout=30,
                    # 分布式模式下 CPU 压力大，适当放宽握手等待
                    skip_utf8_validation=True,
                )
                self._ws = ws
                self._fire_ws("ws_connect", time.monotonic() - start, 0)
                return
            except Exception as exc:
                elapsed = time.monotonic() - start
                if attempt == retries - 1:
                    # 最后一次仍失败才上报错误
                    self._ws = None
                    self._fire_ws("ws_connect", elapsed, 0, exc)
                else:
                    # 短暂等待后重新登录刷新 token 再试
                    time.sleep(1 + attempt)
                    self._login()

    def _close_ws(self):
        if self._ws:
            try:
                self._ws.close()
            except Exception:
                pass
            self._ws = None

    # ── 内部: 事件上报工具 ───────────────────────────────────────────

    def _fire_ws(self, name: str, elapsed_sec: float, length: int, exc=None):
        self.environment.events.request.fire(
            request_type="WS",
            name=name,
            response_time=elapsed_sec * 1000,
            response_length=length,
            exception=exc,
            context={},
        )

    def _fire_http(self, name: str, elapsed_sec: float, length: int, exc=None):
        self.environment.events.request.fire(
            request_type="HTTP",
            name=name,
            response_time=elapsed_sec * 1000,
            response_length=length,
            exception=exc,
            context={},
        )
