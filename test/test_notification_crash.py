#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试通知拉取崩溃问题
"""
import requests
import json

BASE_URL = "http://localhost:8080"

def login(account: str, pwd: str):
    """登录并返回 token"""
    url = f"{BASE_URL}/user/login"
    data = {"account": account, "password": pwd}
    r = requests.post(url, json=data)
    if r.status_code == 200:
        resp = r.json()
        if resp.get("code") == 200:
            return resp.get("data", {}).get("token")
    return None

def get_unread_notifications(token: str):
    """拉取未读通知"""
    url = f"{BASE_URL}/relation/notifications/unread"
    headers = {"Authorization": f"Bearer {token}"}
    r = requests.get(url, headers=headers)
    print(f"Status: {r.status_code}")
    if r.status_code == 200:
        print(f"Response: {r.json()}")
    else:
        print(f"Error: {r.text}")
    return r

def get_all_notifications(token: str):
    """拉取所有通知"""
    url = f"{BASE_URL}/relation/notifications"
    headers = {"Authorization": f"Bearer {token}"}
    r = requests.get(url, headers=headers)
    print(f"Status: {r.status_code}")
    if r.status_code == 200:
        print(f"Response: {r.json()}")
    else:
        print(f"Error: {r.text}")
    return r

if __name__ == "__main__":
    # 使用测试账号
    print("=== 测试通知拉取崩溃问题 ===")
    
    token = login("test1", "123456")
    if not token:
        print("登录失败!")
        exit(1)
    
    print(f"\n登录成功，token: {token[:20]}...")
    
    print("\n=== 测试1: 拉取未读通知 ===")
    get_unread_notifications(token)
    
    print("\n=== 测试2: 拉取所有通知 ===")
    get_all_notifications(token)
    
    print("\n测试完成")
