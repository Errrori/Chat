# 简介
---
### 多人聊天项目

相关资料连接: [项目开发文档](https://gitee.com/lfuturelove/chat2025-docs)
---
### 构建镜像
前置环境：启动docker引擎

1.首先创建一个放置代码的文件夹

2.打开命令行，进入代码文件夹，执行以下命令(drogon-app:latest为镜像名称，可自定义):
```shell
git clone https://gitee.com/supernormal0/chat.git
docker build -t drogon-app:latest .
```
3.此时镜像构建完成，输入命令运行(端口号可以自定义):
```shell
docker run -p 10086:10086 drogon-app:latest
```
---
### 获取更新
1.打开命令行，进入代码文件夹，执行以下命令:
```shell
git pull
docker build -t drogon-app:latest.
```
2.同上，构建完成后运行镜像:
```shell
docker run -p 10086:10086 drogon-app:latest
```
---