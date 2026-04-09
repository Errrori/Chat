#pragma once
#include <chrono>

namespace Heartbeat {

    // ── Drogon transport-level ping/pong ──
    constexpr double PingIntervalSec = 25.0;

    // ── Zombie detection ──
    // 连续 ZombieTimeoutSec 秒无任何客户端消息 → 视为僵尸连接
    constexpr double ZombieTimeoutSec = 90.0;
    // 每隔 MonitorIntervalSec 秒扫描一次所有连接
    constexpr double MonitorIntervalSec = 15.0;

    // ── Token 续期 ──
    // Token 过期前 WarningBeforeExpirySec 秒发送 token_expiring 提醒
    constexpr double WarningBeforeExpirySec = 60.0;
    // Token 过期后允许 RefreshGracePeriodSec 秒内通过 WS 刷新
    constexpr double RefreshGracePeriodSec = 120.0;

    // ── WebSocket 控制消息 type 字段 ──
    namespace MsgType {
        constexpr auto Heartbeat           = "heartbeat";
        constexpr auto HeartbeatAck        = "heartbeat_ack";
        constexpr auto TokenRefresh        = "token_refresh";
        constexpr auto TokenRefreshed      = "token_refreshed";
        constexpr auto TokenRefreshFailed  = "token_refresh_failed";
        constexpr auto TokenExpiring       = "token_expiring";
    }

} // namespace Heartbeat
