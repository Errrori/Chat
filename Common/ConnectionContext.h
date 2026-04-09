#pragma once

#include <string>

struct ConnectionContext
{
    std::string uid;
    trantor::TimerId timer_id;
    std::chrono::time_point<std::chrono::system_clock> expiry;
    std::chrono::time_point<std::chrono::system_clock> last_active_time;
    bool is_delivering_offline{false};
    bool expiry_warned{false};
};
