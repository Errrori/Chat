#pragma once

#include <chrono>
#include <mutex>
#include <string>

struct ConnectionContext
{
    // Immutable after initialization
    std::string uid;

    // Mutable runtime state guarded by mutex
    mutable std::mutex mutex;
    trantor::TimerId timer_id;
    std::chrono::time_point<std::chrono::system_clock> expiry;
    std::chrono::time_point<std::chrono::system_clock> last_active_time;
    bool is_delivering_offline{false};
    bool expiry_warned{false};
};
