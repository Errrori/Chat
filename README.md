# 简介
---
### 项目概述
基于C++17与Drogon框架开发的高并发IM后端，支持私聊、群聊、AI对话场景，通过WebSocket实现低延迟双向通信，HTTP接口处理非长连接业务，完整实现了鉴权、消息存储、会话管理全链路功能。

---
### 项目文档
部分开发文档链接: [项目部分开发文档](https://veryshuai666.github.io/ChatDocs/)

## 项目结构
```
.
├── Common/                 # 公共组件与基础数据结构
├── Data/                   # 数据访问层（数据库操作）
├── Service/                # 业务逻辑层（核心服务）
├── auth/                   # 认证授权模块
├── controllers/            # 控制器层（API路由）
├── models/                 # 数据模型（ORM映射）
├── filter/                 # 过滤器（请求过滤）
├── middleware/             # 中间件（全局处理）
├── static/                 # 静态资源
├── test/                   # 测试用例
├── config.json             # 应用配置文件
├── Container.h/cpp         # 依赖注入容器
├── main.cpp                # 应用入口点
├── Utils.h                 # 通用工具
├── const.h                 # 常量定义
├── pch.h/cpp               # 预编译头文件
└── utils.cpp               # 工具函数实现
```

---
### 项目构建
前置环境：docker
1.首先创建放置项目的文件夹
2.打开命令行，进入代码文件夹，执行以下命令:
```shell
docker compose build
docker compose up -d
```
3.查看运行状态:
```shell
docker compose ps
```
4.查看运行日志:
```shell
docker compose logs -f chat
```