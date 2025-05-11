# 简介
该项目为**2025最强聊天室**，旨在为用户提供一个可以多人同时聊天平台。

贡献者：&nbsp;傅梓炜&nbsp; 、郭俊榮

# docsify准备
#### 1. 安装node.js

#### 2. 全局安装 `docsify` 工具
可以方便地创建及在本地预览生成的文档。
   ``` bash
   npm i docsify-cli -g
   ```

#### 3. 初始化项目（）
在项目的`./docs`目录里写**docsify文档**，通过init命令初始化项目
``` bash
docsify init ./docs
```
会生成一个`./docs`目录，可以看到几个文件：
```
├── index.html 入口文件
├── README.md 会做为主页内容渲染
├── .nojekyll 用于阻止 GitHub Pages 忽略掉下划线开头的文件
```
#### 4. 本地预览
``` bash
docsify serve docs
```




