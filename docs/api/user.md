# 用户管理
# 1.Register 注册

### URL: &nbsp;&nbsp;http://localhost:10086/auth/register

### Method: &nbsp;&nbsp;POST

request params:
``` json
{
  "username": "username",
  "password": "password"
}
```

response (success) : status_code:200OK
``` json
{
  "code":200,
  "uid": "user UID"
}
```

response (fail) :

<span style="color:red">出错可能的情况：</span>

1.request body is empty
``` json
{
    "code":400,
    "message":"request body is empty"
}
```
2.name or password is empty
``` json
{
    "code":400,
    "message":"name or password is empty"
}
```
3.server add user fail (server error)
``` json
{
    "code":501,
    "message":"Server Database error: can not add user"
}
```

# 2.Login 登录

### URL: &nbsp;&nbsp;http://localhost:10086/auth/login

### Method: &nbsp;&nbsp;POST

request params:
``` json
{
  "username": "用户名",
  "password": "密码"
}
```
response (success): status_code:200OK
``` json
{
  "code":200,
  "message":"Login success",
  "token": "this is a monkey token",
  "uid": "this is your uid"
}
```
response (fail):

<span style="color:red">以下为可能出错的情况：</span>

1.user is not exist
``` json
{
    "code":400,
    "message":"Can not find user!"
}
```
2.username is not correct
``` json
{
    "code":401,
    "message":"username is no correct"
}
```
3.password is not correct
``` json
{
    "code":401,
    "message":"password is no correct"
}
```

# 3.修改用户名

### URL: &nbsp;&nbsp;http://localhost:10086/user/modify/name

### Method: &nbsp;&nbsp;POST

request params:
``` json
{
  "uid": "user ID",
  "username": "new user name"
}
```

# 4.修改用户头像

### URL: &nbsp;&nbsp;http://localhost:10086/user/modify/avatar
### Method: &nbsp;&nbsp;POST

请求参数:
``` json
{
  "uid": "user ID",
  "avatar": "new user avatar"
}
```

成功响应
```json
{
    "code":200,
    "message":"operate success"
}
```

失败响应
```json
{
    "code":400,
    "message":"operate fail"
}
```

# 5.获取聊天记录
### URL: &nbsp;&nbsp;http://localhost:10086/chatroom/records(我忘记改了,下个版本会改成http://localhost:10086/user/get_records)
### Method: &nbsp;&nbsp;GET
### 请求参数：（都是URL参数）
1.已有的最新消息id (必须存在) : existing_id
示例 ：http://localhost:10086/chatroom/records?existing_id=id
2.要获取的消息数   (可选) : message_number
示例 ：http://localhost:10086/chatroom/records?existing_id=id&message_num = number
说明：没有message_num时默认返回50条消息
### 成功响应
```json{
  {
    "code": 200,
    "data": [
        {
            "avatar": "https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png",
            "content": "hello1",
            "create_time": "2025-05-11 07:21:37",
            "message_id": 312127089241227264,
            "sender_name": "username1",
            "sender_uid": "465d2144-db00-4f55-8368-8e2436fb3d9a"
        },
        ...
    ],
    "size": size
}
```