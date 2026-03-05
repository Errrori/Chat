#pragma once

namespace ChatEnums {
    // Defines the status of a friend request in the database.
    enum class FriendRequestStatus {
        Pending = 0,
        Accepted = 1,
        Refused = 2
    };

    // Defines the type of a notification.
    enum class NoticeType {
        RequestReceived = 0,
        RequestAccepted = 1,
        RequestRejected = 2
    };

    enum class WebMessageType
    {
        Notice = 0,
        ChatMessage = 1
    };
}
