#pragma once

/// WebSocket 协议类型定义 —— 唯一真相来源
///
/// 规则：
///   - type 字段统一为整数，不使用字符串
///   - 具有子类型的消息（Notice / Error）通过额外的 sub_type 整数字段区分
///   - 同一领域的收发方向用不同枚举值区分（ChatSend vs ChatMessage，AiRequest vs AiResponse 等）
namespace WsMsg {

/// 协议 type 字段值（C→S 入站 / S→C 出站 均用此枚举）
enum class Type : int {
    Heartbeat      = 1,   ///< C→S  心跳请求
    HeartbeatAck   = 2,   ///< S→C  心跳确认
    TokenRefresh   = 3,   ///< C→S  Token 续期请求
    TokenRefreshed = 4,   ///< S→C  Token 续期成功
    ChatSend       = 5,   ///< C→S  客户端发送聊天消息
    ChatMessage    = 6,   ///< S→C  服务器投递聊天消息
    AiRequest      = 7,   ///< C→S  AI 对话请求
    AiResponse     = 8,   ///< S→C  AI 对话响应（chunk / 完成帧）
    Notice         = 9,   ///< S→C  通知推送，sub_type = WsMsg::NoticeSubType
    Error          = 10,  ///< S→C  错误通知，sub_type = WsMsg::ErrorSubType + code = ChatCode::Code
};

/// Notice 子类型（sub_type 字段）
enum class NoticeSubType : int {
    FriendRequestReceived = 0,  ///< 收到好友申请
    FriendRequestAccepted = 1,  ///< 好友申请被接受
    FriendRequestRejected = 2,  ///< 好友申请被拒绝
};

/// Error 子类型（sub_type 字段）
enum class ErrorSubType : int {
    General          = 0,  ///< 通用错误，看 code + error 字段
    TokenExpiring    = 1,  ///< Token 即将过期或已过期，看 expires_in 字段
    TokenRefreshFail = 2,  ///< Token 续期失败，看 error 字段
};

} // namespace WsMsg
