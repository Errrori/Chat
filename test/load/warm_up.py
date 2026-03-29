"""
测试账号预热脚本
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
在运行压测之前执行本脚本，批量预注册所有账号并验证登录，
确保 Locust 启动时无额外注册开销，延迟指标更准确。

用法:
  python warm_up.py --host http://127.0.0.1:10086
  python warm_up.py --host http://127.0.0.1:10086 --size 1000 --workers 50

参数:
  --host     服务器地址   (default: http://127.0.0.1:10086)
  --size     账号池大小   (default: 读取 LOAD_TEST_POOL_SIZE，否则 500)
  --workers  并发线程数   (default: 30)
  --verify   注册后验证登录是否成功 (default: True)
  --timeout  单次请求超时秒数 (default: 10)

输出示例:
  ✔ 注册成功: 498 / 500
  ✗ 注册失败:   2 / 500  (见 failed_accounts.txt)
  ✔ 登录验证: 498 / 498
  完成，耗时 12.3s，平均速率 40.7 账号/s
"""

import argparse
import os
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

sys.path.insert(0, os.path.dirname(__file__))

import requests

from user_pool import ACCOUNT_PREFIX, PASSWORD, POOL_SIZE, account_name

# ── ANSI 颜色（Windows 10+ 支持）──────────────────────────────────
GREEN = "\033[92m"
RED   = "\033[91m"
CYAN  = "\033[96m"
BOLD  = "\033[1m"
RESET = "\033[0m"


def _register_one(base_url: str, idx: int, timeout: int) -> tuple[int, bool, str]:
    """
    注册单个账号。
    返回 (idx, success, message)。
    """
    account = account_name(idx)
    try:
        resp = requests.post(
            f"{base_url}/auth/register",
            json={
                "account": account,
                "password": PASSWORD,
                "username": account,
            },
            timeout=timeout,
        )
        # 200 = 注册成功；400/409 = 账号已存在（视为成功）
        ok = resp.status_code in (200, 400, 409)
        msg = "" if ok else f"HTTP {resp.status_code}: {resp.text[:80]}"
        return idx, ok, msg
    except Exception as exc:
        return idx, False, str(exc)


def _login_one(base_url: str, idx: int, timeout: int) -> tuple[int, bool, str]:
    """
    登录单个账号，验证能否取得 token。
    返回 (idx, success, message)。
    """
    account = account_name(idx)
    try:
        resp = requests.post(
            f"{base_url}/auth/login",
            json={"account": account, "password": PASSWORD},
            timeout=timeout,
        )
        if resp.status_code == 200:
            data = resp.json().get("data", {})
            token = data.get("token", "")
            ok = bool(token)
            msg = "" if ok else "token missing in response"
        else:
            ok = False
            msg = f"HTTP {resp.status_code}: {resp.text[:80]}"
        return idx, ok, msg
    except Exception as exc:
        return idx, False, str(exc)


def _run_phase(
    label: str,
    func,
    indices: list[int],
    base_url: str,
    max_workers: int,
    timeout: int,
) -> tuple[list[int], list[tuple[int, str]]]:
    """
    并发执行 func(base_url, idx, timeout)，显示进度条，
    返回 (成功 idx 列表, 失败 [(idx, msg)] 列表)。
    """
    total = len(indices)
    successes: list[int] = []
    failures: list[tuple[int, str]] = []
    done = 0

    with ThreadPoolExecutor(max_workers=max_workers) as pool:
        futures = {pool.submit(func, base_url, idx, timeout): idx for idx in indices}
        for future in as_completed(futures):
            idx, ok, msg = future.result()
            done += 1
            if ok:
                successes.append(idx)
            else:
                failures.append((idx, msg))

            # 进度条（覆写同一行）
            pct = done / total * 100
            bar = "█" * int(pct / 2) + "░" * (50 - int(pct / 2))
            print(
                f"\r  {label} [{bar}] {done}/{total} ({pct:.1f}%)",
                end="",
                flush=True,
            )

    print()  # 换行
    return successes, failures


def main():
    parser = argparse.ArgumentParser(description="ChatServer 测试账号预热")
    parser.add_argument("--host",    default="http://127.0.0.1:10086")
    parser.add_argument("--size",    default=None, type=int,
                        help=f"账号数量（默认用 LOAD_TEST_POOL_SIZE 环境变量，否则 {POOL_SIZE}）")
    parser.add_argument("--workers", default=30, type=int, help="并发线程数")
    parser.add_argument("--timeout", default=10, type=int, help="单次请求超时秒数")
    parser.add_argument("--no-verify", action="store_true",
                        help="跳过注册后的登录验证步骤")
    args = parser.parse_args()

    pool_size: int = args.size or POOL_SIZE
    base_url: str  = args.host.rstrip("/")
    max_workers: int = args.workers
    timeout: int   = args.timeout

    print(f"\n{BOLD}ChatServer 测试账号预热{RESET}")
    print(f"  目标服务: {CYAN}{base_url}{RESET}")
    print(f"  账号数量: {pool_size}  (前缀: {ACCOUNT_PREFIX}, 密码: {PASSWORD})")
    print(f"  并发线程: {max_workers}  超时: {timeout}s\n")

    all_indices = list(range(pool_size))
    t0 = time.monotonic()

    # ── Phase 1: 注册 ────────────────────────────────────────────────
    reg_success, reg_fail = _run_phase(
        "注册", _register_one, all_indices, base_url, max_workers, timeout
    )

    if reg_fail:
        print(f"  {RED}✗ 注册失败: {len(reg_fail):4d} / {pool_size}{RESET}")
        fail_file = os.path.join(os.path.dirname(__file__), "failed_accounts.txt")
        with open(fail_file, "w", encoding="utf-8") as f:
            for idx, msg in sorted(reg_fail):
                f.write(f"{account_name(idx)}: {msg}\n")
        print(f"    失败账号已写入: {fail_file}")
    else:
        print(f"  {GREEN}✔ 注册成功: {len(reg_success):4d} / {pool_size}{RESET}")

    # ── Phase 2: 登录验证 ─────────────────────────────────────────────
    if not args.no_verify and reg_success:
        print()
        login_success, login_fail = _run_phase(
            "登录验证", _login_one, reg_success, base_url, max_workers, timeout
        )

        if login_fail:
            print(f"  {RED}✗ 登录失败: {len(login_fail):4d} / {len(reg_success)}{RESET}")
            for idx, msg in login_fail[:5]:
                print(f"    {account_name(idx)}: {msg}")
            if len(login_fail) > 5:
                print(f"    … 及其他 {len(login_fail) - 5} 个")

            # 检测"密码不正确"错误：通常意味着 DB 中残留了旧的明文密码（服务端密码
            # 哈希算法已改为 MD5，但老账号数据未迁移）。
            stale_pw = [idx for idx, msg in login_fail if "password is not correct" in msg]
            if stale_pw:
                print(f"\n  {BOLD}[诊断] 检测到 {len(stale_pw)} 个账号密码格式不匹配（可能是旧明文密码残留）{RESET}")
                print(f"  运行以下命令清除旧测试数据后重新预热：\n")
                print(f"    docker compose exec postgres psql -U postgres -d postgres \\")
                print(f"      -c \"DELETE FROM users WHERE account LIKE '{ACCOUNT_PREFIX}%';\"")
                print(f"\n  然后再次运行: python warm_up.py --host {base_url}\n")
        else:
            print(f"  {GREEN}✔ 登录验证: {len(login_success):4d} / {len(reg_success)}{RESET}")

    # ── 汇总 ──────────────────────────────────────────────────────────
    elapsed = time.monotonic() - t0
    rate = pool_size / elapsed if elapsed > 0 else 0
    print(f"\n  {BOLD}完成{RESET}，耗时 {elapsed:.1f}s，平均速率 {rate:.1f} 账号/s")

    if reg_fail or (not args.no_verify and login_fail):
        sys.exit(1)


if __name__ == "__main__":
    main()
