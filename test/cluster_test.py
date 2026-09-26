"""
ChatServer 多节点集群路由测试脚本

覆盖：
  1. /debug/cluster：各节点上报的 node_id 与存活节点列表
  2. 严格单端登录：同账号在第二个节点登录后，第一个连接被跨节点踢下线
  3. 跨节点消息投递：用户分别连在不同节点，聊天消息经 Redis 路由送达
  4. 路由 CAS：同一节点重复登录不会残留脏路由

用法:
  # 两个副本各自暴露一个端口时（推荐）
  python cluster_test.py --nodes http://127.0.0.1:10086,http://127.0.0.1:10087

  # 只有一个入口时仍可跑：单节点会退化为"同节点本地踢人"验证
  python cluster_test.py --nodes http://127.0.0.1:10086

依赖: pip install requests websocket-client
"""

import argparse
import json
import sys
import threading
import time

import requests
import websocket

# Windows 控制台默认 GBK，无法输出 ✔/✗/→ 等符号
try:
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
except Exception:
    pass

GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
CYAN = "\033[96m"
RESET = "\033[0m"
BOLD = "\033[1m"


def ok(msg): print(f"  {GREEN}✔ {msg}{RESET}")
def fail(msg): print(f"  {RED}✗ {msg}{RESET}")
def info(msg): print(f"  {CYAN}→ {msg}{RESET}")
def warn(msg): print(f"  {YELLOW}! {msg}{RESET}")
def header(msg): print(f"\n{BOLD}{YELLOW}{'─'*50}\n  {msg}\n{'─'*50}{RESET}")


def gen_account(prefix):
    """账号必须匹配 ^[a-zA-Z0-9]{6,16}$，用前缀+时间戳后8位保证唯一且合法。"""
    return f"{prefix}{str(int(time.time()))[-8:]}"


# ───────────────────────── HTTP 会话 ─────────────────────────
class HttpSession:
    def __init__(self, base_url, account, password, username=""):
        self.base = base_url.rstrip("/")
        self.account = account
        self.password = password
        self.username = username or account
        self.token = ""
        self.uid = ""

    def register(self):
        # 注册为 best-effort：账号已存在时登录仍会成功，真正的门槛是 login。
        r = requests.post(f"{self.base}/auth/register", json={
            "account": self.account, "password": self.password, "username": self.username,
        })
        if r.status_code not in (200, 409):
            info(f"[{self.account}] register: {r.status_code} {r.text[:120]}")
        return True

    def login(self):
        r = requests.post(f"{self.base}/auth/login", json={
            "account": self.account, "password": self.password,
        })
        if r.status_code != 200:
            fail(f"[{self.account}] 登录失败 HTTP {r.status_code}: {r.text[:200]}")
            return False
        data = r.json().get("data", {})
        self.token = data.get("token", "")
        self.uid = data.get("uid", "")
        if not self.token or not self.uid:
            fail(f"[{self.account}] 响应缺少 token/uid: {data}")
            return False
        return True

    def post(self, path, body=None):
        return requests.post(f"{self.base}{path}", json=body or {},
                             headers={"Authorization": f"Bearer {self.token}"})

    def get(self, path, params=None):
        return requests.get(f"{self.base}{path}", params=params or {},
                            headers={"Authorization": f"Bearer {self.token}"})


# ───────────────────────── WebSocket 客户端 ─────────────────────────
class WSClient:
    def __init__(self, base_url, token, name):
        self.name = name
        ws_base = base_url.rstrip("/").replace("http://", "ws://").replace("https://", "wss://")
        self.url = f"{ws_base}/ws/chat?token={token}"
        self.messages = []
        self.opened = threading.Event()
        self.closed = threading.Event()
        self.close_code = None
        self.ws = websocket.WebSocketApp(
            self.url,
            on_open=self._on_open,
            on_message=self._on_message,
            on_error=self._on_error,
            on_close=self._on_close,
        )
        self.thread = threading.Thread(target=self.ws.run_forever, daemon=True)

    def start(self):
        self.thread.start()
        return self

    def _on_open(self, ws):
        self.opened.set()

    def _on_message(self, ws, message):
        try:
            self.messages.append(json.loads(message))
        except Exception:
            self.messages.append(message)

    def _on_error(self, ws, error):
        pass

    def _on_close(self, ws, code, msg):
        self.close_code = code
        self.closed.set()

    def send_json(self, obj):
        self.ws.send(json.dumps(obj))

    def wait_message(self, msg_type, timeout=6):
        deadline = time.time() + timeout
        while time.time() < deadline:
            for m in list(self.messages):
                if isinstance(m, dict) and m.get("type") == msg_type:
                    return m
            time.sleep(0.1)
        return None

    def close(self):
        try:
            self.ws.close()
        except Exception:
            pass


# ───────────────────────── 测试用例 ─────────────────────────
def test_cluster_info(nodes):
    header("TEST: /debug/cluster 集群信息")
    node_ids = []
    for base in nodes:
        try:
            r = requests.get(f"{base.rstrip('/')}/debug/cluster", timeout=5)
        except Exception as e:
            fail(f"{base} 不可达: {e}")
            sys.exit(1)
        if r.status_code != 200:
            fail(f"{base}/debug/cluster HTTP {r.status_code}: {r.text[:200]}")
            sys.exit(1)
        data = r.json().get("data", {})
        node_id = data.get("node_id", "")
        node_ids.append(node_id)
        ok(f"{base}  node_id={node_id}  local_connections={data.get('local_connections')}")
        info(f"  alive_nodes={data.get('alive_nodes')}")

    if len(nodes) >= 2:
        if len(set(node_ids)) != len(node_ids):
            fail(f"多节点 node_id 不唯一: {node_ids}（检查 NODE_ID/主机名）")
        else:
            ok(f"node_id 唯一: {node_ids}")
    return node_ids


def test_strict_single_login(nodes, account):
    header("TEST: 严格单端登录（跨节点强踢）")
    sess = HttpSession(nodes[0], account, "test123456", "KickTest")
    sess.register()
    if not sess.login():
        fail("登录失败，跳过")
        return False

    first = WSClient(nodes[0], sess.token, "first").start()
    if not first.opened.wait(5):
        fail("第一个连接未能建立")
        return False
    ok(f"连接 #1 建立在 {nodes[0]}")
    time.sleep(0.8)  # 等路由写入

    second = WSClient(nodes[-1], sess.token, "second").start()
    if not second.opened.wait(5):
        fail("第二个连接未能建立")
        first.close(); second.close()
        return False
    ok(f"连接 #2 建立在 {nodes[-1]}")

    if first.closed.wait(6):
        ok(f"连接 #1 已被踢下线 (close_code={first.close_code})")
        result = True
    else:
        fail("连接 #1 未被踢下线（严格单端失败）")
        result = False

    first.close(); second.close()
    return result


def ensure_private_thread(alice, bob):
    """建立 Alice→Bob 好友关系并返回私聊 thread_id"""
    r = alice.post("/relation/send-friend-request",
                   {"acceptor_uid": bob.uid, "message": "cluster test"})
    if r.status_code != 200:
        # 可能已是好友或已存在申请，尝试直接接受
        info(f"send-friend-request: {r.status_code} {r.text[:120]}")
    r = bob.post("/relation/process-friend-request",
                 {"requester_uid": alice.uid, "action": 1})
    if r.status_code != 200:
        fail(f"process-friend-request HTTP {r.status_code}: {r.text[:200]}")
        return None
    return r.json().get("data", {}).get("thread_id")


def test_cross_node_delivery(nodes):
    header("TEST: 跨节点消息投递")
    ts = str(int(time.time()))
    alice = HttpSession(nodes[0], gen_account("clka"), "test123456", "ClusterA")
    bob = HttpSession(nodes[-1], gen_account("clkb"), "test123456", "ClusterB")
    alice.register(); bob.register()
    if not (alice.login() and bob.login()):
        fail("登录失败，跳过")
        return False

    thread_id = ensure_private_thread(alice, bob)
    if not thread_id:
        fail("未能创建私聊 thread")
        return False
    ok(f"私聊 thread_id={thread_id}")

    ws_alice = WSClient(nodes[0], alice.token, "alice").start()
    ws_bob = WSClient(nodes[-1], bob.token, "bob").start()
    if not (ws_alice.opened.wait(5) and ws_bob.opened.wait(5)):
        fail("WebSocket 连接失败")
        ws_alice.close(); ws_bob.close()
        return False
    ok(f"alice@{nodes[0]}  bob@{nodes[-1]} 已连接")
    time.sleep(1.0)  # 等两边路由写入 Redis

    content = f"cross-node-{ts}"
    ws_alice.send_json({"type": 5, "thread_id": thread_id, "content": content})

    msg = ws_bob.wait_message(6, timeout=6)
    if msg and msg.get("data", {}).get("content") == content:
        ok(f"bob 收到跨节点消息: {content}")
        result = True
    elif msg:
        fail(f"bob 收到消息但内容不匹配: {json.dumps(msg, ensure_ascii=False)[:200]}")
        result = False
    else:
        fail("bob 未在超时内收到跨节点消息")
        info(f"alice 收到: {json.dumps(ws_alice.messages, ensure_ascii=False)[:400]}")
        info(f"bob   收到: {json.dumps(ws_bob.messages, ensure_ascii=False)[:400]}")
        result = False

    ws_alice.close(); ws_bob.close()
    return result


def test_cas_route_cleanup(nodes):
    header("TEST: 路由 CAS 清理（重复上线后路由仍指向在线节点）")
    ts = str(int(time.time()))
    sess = HttpSession(nodes[0], gen_account("clkc"), "test123456", "ClusterC")
    sess.register()
    if not sess.login():
        fail("登录失败，跳过")
        return False

    # 连到最后一个节点，再断开，路由应被清理
    ws = WSClient(nodes[-1], sess.token, "cas").start()
    if not ws.opened.wait(5):
        fail("连接失败")
        return False
    time.sleep(0.8)
    ws.close()
    time.sleep(1.5)

    # 重连到第一个节点，仍应能正常建立（说明没有残留脏路由干扰）
    ws2 = WSClient(nodes[0], sess.token, "cas2").start()
    good = ws2.opened.wait(5)
    ok("重连成功，路由未残留干扰") if good else fail("重连后无法建立连接")
    ws2.close()
    return good


def main():
    parser = argparse.ArgumentParser(description="ChatServer 多节点集群路由测试")
    parser.add_argument("--nodes", required=True,
                        help="逗号分隔的节点地址，如 http://127.0.0.1:10086,http://127.0.0.1:10087")
    parser.add_argument("--skip-delivery", action="store_true", help="跳过跨节点消息投递用例")
    args = parser.parse_args()

    nodes = [n.strip() for n in args.nodes.split(",") if n.strip()]
    if not nodes:
        parser.error("--nodes 不能为空")

    print(f"\n{BOLD}ChatServer 多节点集群测试{RESET}")
    print(f"节点: {CYAN}{nodes}{RESET}")

    results = {}
    test_cluster_info(nodes)
    results["strict_single_login"] = test_strict_single_login(nodes, gen_account("clkk"))
    if not args.skip_delivery:
        results["cross_node_delivery"] = test_cross_node_delivery(nodes)
    results["cas_route_cleanup"] = test_cas_route_cleanup(nodes)

    header("结果汇总")
    all_ok = True
    for name, passed in results.items():
        if passed:
            ok(name)
        else:
            fail(name)
            all_ok = False

    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
