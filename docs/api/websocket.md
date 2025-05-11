# **Websocket**

## **聊天室功能**
### *URL*: &nbsp;&nbsp;ws://localhost:10086/ws/chatroom

### *请求参数*：token,可以用三种方式传递
#### 1.Authorization:
格式 ：JWT token
说明：JWT加上空格，空格后面加上token(记住这里才需要"JWT加上空格" 这个前缀，后面均不需要)
#### 2.请求体中Json的token字段
格式：
```json
{
    "token" : "token"
}
```
#### 3.URL参数token
格式：ws://localhost:10086/ws/chatroom?token=token

### *通信格式*：
#### 客户端发送消息的格式：
```json
{
    "id" : 临时id,
    "content" : 消息内容
}
```
#### 服务器收到消息后会返回：
```json
{
    "id": 客户端的临时id，
    "message_id": 服务端的消息id，
    "ack": true   标识这是一条回复信息，服务端成功收到消息
}
```
#### 其他客户端(除发送消息的客户端)收到消息的格式：
```json
{
    "id" : 临时id,
    "content" : 消息内容
    "sender_uid": 发送消息的用户的uid
    "sender_name": 发送消息的用户的name
    "message_id": 服务端的消息id
}
```
