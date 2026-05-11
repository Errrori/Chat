#pragma once

namespace ChatEnums {
    // Defines the status of a friend request in the database.
    enum class FriendRequestStatus {
        Pending = 0,
        Accepted = 1,
        Refused = 2
    };
}
