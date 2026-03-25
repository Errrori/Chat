#pragma once

#include <string>

struct ConnectionContext
{
    std::string uid;
    trantor::TimerId timer_id;
    std::chrono::time_point<std::chrono::system_clock> expiry;
};
