"""
ChatServer API 自动化测试脚本
用法: python api_test.py [--host 127.0.0.1] [--port 10086]
依赖: pip install requests websocket-client
"""

import requests
import json
import sys
import argparse
import time
import threading
import websocket

# ─────────────────────────── 颜色输出 ───────────────────────────
GREEN  = "\033[92m"
RED    = "\033[91m"
YELLOW = "\033[93m"
CYAN   = "\033[96m"
RESET  = "\033[0m"
BOLD   = "\033[1m"

def ok(msg):   print(f"  {GREEN}✔ {msg}{RESET}")
def fail(msg): print(f"  {RED}✗ {msg}{RESET}")
def info(msg): print(f"  {CYAN}→ {msg}{RESET}")
def header(msg): print(f"\n{BOLD}{YELLOW}{'─'*50}\n  {msg}\n{'─'*50}{RESET}")


# ───────────────────────── 用户会话类 ─────────────────────────── 
class UserSession:
    """封装单个用户的 token/uid，自动附带 Authorization 头"""
    def __init__(self, base_url: str, account: str, password: str, username: str = ""):
        self.base      = base_url
        self.account   = account
        self.password  = password
        self.username  = username or account
        self.token: str = ""
        self.uid: str   = ""

    # ── 注册（已注册则忽略） ──
    def register(self):
        r = requests.post(f"{self.base}/auth/register", json={
            "account": self.account,
            "password": self.password,
            "username": self.username,
        })
        # 409 / 重复账号都算 OK
        return r.status_code in (200, 400, 409)

    # ── 登录，成功后保存 token & uid ──
    def login(self) -> bool:
        r = requests.post(f"{self.base}/auth/login", json={
            "account": self.account,
            "password": self.password,
        })
        if r.status_code != 200:
            fail(f"[{self.account}] 登录失败 HTTP {r.status_code}: {r.text}")
            return False
        data = r.json()
        self.token = data.get("data", {}).get("token", "")
        self.uid   = data.get("data", {}).get("uid", "")
        if not self.token or not self.uid:
            fail(f"[{self.account}] 响应缺少 token/uid: {data}")
            return False
        ok(f"[{self.account}] 登录成功  uid={self.uid[:12]}…")
        return True

    # ── 带 token 的 POST/GET 快捷方法 ──
    def post(self, path: str, body: dict = None) -> requests.Response:
        return requests.post(
            f"{self.base}{path}",
            json=body or {},
            headers={"Authorization": f"Bearer {self.token}"},
        )

    def get(self, path: str, params: dict = None) -> requests.Response:
        return requests.get(
            f"{self.base}{path}",
            params=params or {},
            headers={"Authorization": f"Bearer {self.token}"},
        )

    # ── WebSocket 连接（异步接收，打印通知） ──
    def connect_ws(self, ws_url: str):      
        def on_message(ws, message):
            try:
                data = json.loads(message)
                info(f"[{self.account}] WS收到: {json.dumps(data, ensure_ascii=False, indent=2)}")
            except Exception:
                info(f"[{self.account}] WS raw: {message}")

        def on_error(ws, error):
            fail(f"[{self.account}] WS错误: {error}")

        def on_open(ws):
            ok(f"[{self.account}] WebSocket 已连接")

        url = f"{ws_url}?token={self.token}"
        ws = websocket.WebSocketApp(url, on_message=on_message, on_error=on_error, on_open=on_open)
        t = threading.Thread(target=ws.run_forever, daemon=True)
        t.start()
        return ws


# ─────────────────────── 断言辅助 ─────────────────────────────
def assert_ok(resp: requests.Response, case_name: str) -> dict:
    """断言 HTTP 200，返回 data 字段，失败则打印并返回 None"""
    if resp.status_code == 200:
        data = resp.json()
        ok(f"{case_name}  → {json.dumps(data, ensure_ascii=False)[:120]}")
        return data
    else:
        fail(f"{case_name}  HTTP {resp.status_code}: {resp.text[:200]}")
        return None


# ═══════════════════════════ 测试用例 ════════════════════════════

def test_register_and_login(alice: UserSession, bob: UserSession):
    header("TEST: 注册 & 登录")
    alice.register()
    bob.register()
    a_ok = alice.login()
    b_ok = bob.login()
    assert a_ok and b_ok, "登录失败，后续测试中止"

# ─────────────────────────────────────────────────────────────────
def test_get_user(alice: UserSession):
    header("TEST: 查询用户信息")
    r = alice.get("/user/get-user", {"uid": alice.uid})
    assert_ok(r, "get-user(self)")

# ─────────────────────────────────────────────────────────────────
def test_friend_request_flow(alice: UserSession, bob: UserSession) -> int | None:
    """alice 向 bob 发送好友请求，bob 接受，返回 thread_id"""
    header("TEST: 好友请求流程 (Alice→Bob→接受)")

    # 1. Alice 发送请求
    r = alice.post("/relation/send-friend-request", {
        "acceptor_uid": bob.uid,
        "message": "Hi Bob, 我是 Alice！",
    })
    assert_ok(r, "send-friend-request")

    # 2. Bob 接受请求
    r = bob.post("/relation/process-friend-request", {
        "requester_uid": alice.uid,
        "action": 1,           # 1=接受  2=拒绝
    })
    result = assert_ok(r, "process-friend-request(accept)")
    if result:
        thread_id = result.get("data", {}).get("thread_id")
        info(f"私聊 thread_id = {thread_id}")
        return thread_id
    return None

# ─────────────────────────────────────────────────────────────────
def test_friend_request_reject(alice: UserSession, charlie: UserSession):
    header("TEST: 好友请求流程 (Alice→Charlie→拒绝)")
    alice.post("/relation/send-friend-request", {
        "acceptor_uid": charlie.uid,
        "message": "Hi Charlie！",
    })
    r = charlie.post("/relation/process-friend-request", {
        "requester_uid": alice.uid,
        "action": 2,           # 2=拒绝
    })
    assert_ok(r, "process-friend-request(reject)")

# ─────────────────────────────────────────────────────────────────
def test_create_group_thread(alice: UserSession, bob: UserSession):
    header("TEST: 创建群聊")
    r = alice.post("/thread/create/group-chat", {
        "name": "Test Group",
        "description": "自动化测试群",
    })
    result = assert_ok(r, "create-group-chat")
    if result:
        tid = result.get("data", {}).get("thread_id")
        # Alice 邀请 Bob
        r2 = alice.post("/thread/group/add-member", {
            "thread_id": tid,
            "user_uid": bob.uid,
        })
        assert_ok(r2, "add-member(bob)")
        return tid

# ─────────────────────────────────────────────────────────────────
def test_create_ai_thread(alice: UserSession):
    header("TEST: 创建 AI 对话")
    r = alice.post("/thread/create/ai-chat", {
        "name": "AI Test Chat",
    })
    assert_ok(r, "create-ai-chat")

# ─────────────────────────────────────────────────────────────────
def test_get_overview(alice: UserSession):
    header("TEST: 获取会话概览")
    r = alice.get("/thread/record/overview")
    assert_ok(r, "thread/record/overview")

# ─────────────────────────────────────────────────────────────────
def test_get_private_records(alice: UserSession, thread_id: int):
    header("TEST: 获取私聊记录")
    if not thread_id:
        info("跳过（无 thread_id）")
        return
    r = alice.get("/thread/record/user", {"thread_id": thread_id})
    assert_ok(r, "thread/record/user")

# ─────────────────────────────────────────────────────────────────
def test_get_unread_notifications(user: UserSession):
    header(f"TEST: 拉取未读通知 ({user.account})")
    r = user.get("/relation/notifications/unread")
    result = assert_ok(r, "notifications/unread")
    if result:
        items = result.get("data", {}).get("items", [])
        unread_ids = [item["id"] for item in items if isinstance(item.get("id"), int)]
        info(f"未读通知数: {len(items)}, ids={unread_ids[:5]}")
        return unread_ids
    return []

# ─────────────────────────────────────────────────────────────────
def test_get_notifications(user: UserSession, offset: int = 0, limit: int = 20):
    header(f"TEST: 拉取所有通知 ({user.account}, offset={offset}, limit={limit})")
    r = user.get("/relation/notifications", {"offset": offset, "limit": limit})
    result = assert_ok(r, "notifications(all)")
    if result:
        info(f"本页条数: {result.get('data', {}).get('count', 0)}")

# ─────────────────────────────────────────────────────────────────
def test_mark_notifications_read(user: UserSession, ids: list):
    header(f"TEST: 批量标记已读 ({user.account})")
    # 1. 按指定 ID 标记
    if ids:
        r = user.post("/relation/notifications/mark-read", {"ids": ids[:3]})
        result = assert_ok(r, f"mark-read(ids={ids[:3]})")
        if result:
            info(f"按 ID 标记影响行数: {result.get('data', {}).get('updated', 0)}")

    # 2. 空 ids = 标记全部未读
    r = user.post("/relation/notifications/mark-read", {})
    result = assert_ok(r, "mark-read(all)")
    if result:
        info(f"全量标记影响行数: {result.get('data', {}).get('updated', 0)}")

    # 3. 验证：再拉未读应为 0
    r = user.get("/relation/notifications/unread")
    result = assert_ok(r, "unread after mark-read")
    if result:
        count = result.get("data", {}).get("count", -1)
        if count == 0:
            ok(f"未读清零确认通过")
        else:
            fail(f"标记已读后未读仍为 {count}")

# ─────────────────────────────────────────────────────────────────
def test_get_pending_friend_requests(user: UserSession):
    header(f"TEST: 拉取待处理好友申请 ({user.account})")
    r = user.get("/relation/friend-requests")
    result = assert_ok(r, "friend-requests(pending)")
    if result:
        items = result.get("data", {}).get("items", [])
        info(f"待处理申请数: {len(items)}")
        for item in items[:3]:
            info(f"  requester_uid={item.get('requester_uid')}  payload={item.get('payload')}")

# ─────────────────────────────────────────────────────────────────
def test_websocket_connect(alice: UserSession, bob: UserSession, ws_url: str):
    header("TEST: WebSocket 连接 & 互发消息 (5s)")
    ws_alice = alice.connect_ws(ws_url)
    ws_bob   = bob.connect_ws(ws_url)
    time.sleep(1)   # 等连接建立
    info("WebSocket 连接已保持 5 秒，观察通知输出…")
    time.sleep(5)
    ws_alice.close()
    ws_bob.close()


# ═══════════════════════════ 主入口 ══════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="ChatServer 自动化测试")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", default=10086, type=int)
    parser.add_argument("--ws",   action="store_true", help="同时测试 WebSocket")
    args = parser.parse_args()

    base   = f"http://{args.host}:{args.port}"
    ws_url = f"ws://{args.host}:{args.port}/chat"

    print(f"\n{BOLD}ChatServer API 自动化测试{RESET}")
    print(f"目标: {CYAN}{base}{RESET}")

    # ── 预置两个固定测试账号 ──
    alice   = UserSession(base, "test_alice",   "test123456", "Alice")
    bob     = UserSession(base, "test_bob",     "test123456", "Bob")
    charlie = UserSession(base, "test_charlie", "test123456", "Charlie")

    try:
        # 1. 注册 & 登录
        test_register_and_login(alice, bob)
        charlie.register()
        charlie.login()

        # 2. 查询用户
        test_get_user(alice)

        # 3. 好友请求流程
        thread_id = test_friend_request_flow(alice, bob)

        # 4. 拒绝请求
        test_friend_request_reject(alice, charlie)

        # 5. 群聊
        test_create_group_thread(alice, bob)

        # 6. AI 对话
        test_create_ai_thread(alice)

        # 7. 会话概览
        test_get_overview(alice)

        # 8. 私聊记录
        test_get_private_records(alice, thread_id)

        # 9. 新功能：通知相关
        # Dave 向 alice 发一个好友请求，制造一条 alice 的未读通知
        dave = UserSession(base, "test_dave", "test123456", "Dave")
        dave.register()
        dave.login()
        dave.post("/relation/send-friend-request", {
            "acceptor_uid": alice.uid,
            "message": "Hi Alice from Dave",
        })

        unread_ids = test_get_unread_notifications(alice)
        test_get_notifications(alice)
        test_mark_notifications_read(alice, unread_ids)

        # 10. Bob 向 charlie 发请求，测试 charlie 拉取待处理申请
        bob.post("/relation/send-friend-request", {
            "acceptor_uid": charlie.uid,
            "message": "Hi Charlie from Bob",
        })
        test_get_pending_friend_requests(charlie)

        # 11. WebSocket（可选）
        if args.ws:
            test_websocket_connect(alice, bob, ws_url)

    except AssertionError as e:
        fail(str(e))
        sys.exit(1)

    header("全部测试完成")


if __name__ == "__main__":
    main()
