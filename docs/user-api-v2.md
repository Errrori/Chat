# 用户接口 V2（读模型拆分）

## 目标
- 内部高频链路只依赖 `uid -> username/avatar`（display profile）
- 用户搜索和资料查询走显式接口，不再被缓存字段集合限制
- 资料修改支持 `username/avatar/signature`

## 新接口

### 1) 获取用户资料（按 uid）
- 方法：`GET /user/profile?uid={uid}`
- 说明：返回完整资料（公开侧）
- 成功返回 `data`：
```json
{
  "uid": "...",
  "account": "...",
  "username": "...",
  "avatar": "...",
  "signature": "..."
}
```

### 2) 搜索用户（按 account）
- 方法：`GET /user/search?account={account}`
- 说明：用于添加好友搜索
- 成功返回 `data`：
```json
{
  "uid": "...",
  "username": "...",
  "avatar": "...",
  "signature": "..."
}
```

### 3) 修改当前用户资料（需登录）
- 方法：`POST /user/profile`
- 过滤器：`TokenVerifyFilter`
- 说明：从 token 里取当前用户 `uid`，不接受通过 body 指定任意 uid/account
- 请求 body（至少一个字段）：
```json
{
  "username": "...",
  "avatar": "...",
  "signature": "..."
}
```
- 成功返回 `data`：
```json
{
  "uid": "...",
  "account": "...",
  "username": "...",
  "avatar": "...",
  "signature": "..."
}
```

## 兼容路由（过渡）
- `GET /user/get-user`
  - 若带 `uid` 参数：转到 `GET /user/profile?uid=...`
  - 若带 `account` 参数：转到 `GET /user/search?account=...`
- `POST /user/modify/info`
  - 转到 `POST /user/profile`

## 缓存策略（当前实现）
- Redis 仅缓存 display profile：`username/avatar`
- key 维度：`uid`
- `username/avatar` 变更会失效 display cache
- 仅 `signature` 变更不会触发 display cache 失效
