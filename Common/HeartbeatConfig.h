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

} // namespace Heartbeat
