"""
测试账号池
─────────────────────────────────────────────────────────────────
为各 Locust User 分配唯一测试账号，支持并发安全的循环取号。

账号格式: lt_user_{NNNN}   例: lt_user_0001
密码:     LoadTest@123       （可通过 LOAD_TEST_PASSWORD 环境变量覆盖）

支持的环境变量:
  LOAD_TEST_POOL_SIZE       账号池大小      (default: 500)
  LOAD_TEST_ACCOUNT_PREFIX  账号名前缀      (default: lt_user_)
  LOAD_TEST_PASSWORD        统一密码        (default: LoadTest@123)

用法:
    from user_pool import checkout

    account, password = checkout()
"""

import os
import threading

POOL_SIZE: int = int(os.environ.get("LOAD_TEST_POOL_SIZE", "500"))
ACCOUNT_PREFIX: str = os.environ.get("LOAD_TEST_ACCOUNT_PREFIX", "ltuser")
PASSWORD: str = os.environ.get("LOAD_TEST_PASSWORD", "LoadTest@123")

_counter: int = 0
_lock = threading.Lock()


def checkout() -> tuple[str, str]:
    """
    分配下一个测试账号（循环复用，多 worker 并发安全）。
    返回 (account, password) 元组。

    当并发用户数超过 POOL_SIZE 时，账号会被多个 User 共享——
    这模拟了同一账号在多端登录的场景，对压测无副作用。
    """
    global _counter
    with _lock:
        idx = _counter % POOL_SIZE
        _counter += 1
    return f"{ACCOUNT_PREFIX}{idx:04d}", PASSWORD


def account_name(idx: int) -> str:
    """根据索引生成账号名，用于预热脚本"""
    return f"{ACCOUNT_PREFIX}{idx:04d}"
