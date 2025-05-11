# 调试
# 1.获取数据库信息

### URL: &nbsp;&nbsp;http://localhost:10086/debug/db_info

### Method: &nbsp;&nbsp;GET

response (success): status_code:200OK
``` json
{
  "code": 200,
  "message": "success to get database",
  "data": {
    "table": {
      "username": "users",
      "columns": [
        {"username": "id", "type": "INTEGER", "notnull": true, "pk": true},
        {"username": "name", "type": "TEXT", "notnull": true, "pk": false},
        {"username": "uid", "type": "TEXT", "notnull": true, "pk": false},
        {"username": "password", "type": "TEXT", "notnull": true, "pk": false},
        {"username": "create_time", "type": "TEXT", "notnull": true, "pk": false}
      ],
      "data": [user data vector],
      "total_rows": number of users
    }
  }
}
```
response (fail): status_code:500 InternalServerError

# 2.获取聊天记录(刚加入聊天室或者没有最近的消息id时也可以用这个路由获取聊天记录)
### URL: &nbsp;&nbsp;http://localhost:10086/debug/rd_info
### Method: &nbsp;&nbsp;GET
### 请求参数（可选）: message_number
示例 ：http://localhost:10086/debug/rd_info?message_num = number
说明：没有message_num时默认返回50条消息
### 成功响应
```json{
  "code": 200,
  "data": [
    {
      "avatar": "https://cube.elemecdn.com/0/88/03b0d39583f48206768a7534e55bcpng.png",
      "content": "6",
      "create_time": "2025-05-11 07:25:06",
      "message_id": 312127967004196860,
      "sender_name": "username1",
      "sender_uid": "465d2144-db00-4f55-8368-8e2436fb3d9a"
    },
    ...,
  "size": 8
}
```
### 失败响应
说明：服务器查找记录失败时才会返回这个响应，一般是服务器内部错误
返回体内容："Sorry,no records store in the database. "


