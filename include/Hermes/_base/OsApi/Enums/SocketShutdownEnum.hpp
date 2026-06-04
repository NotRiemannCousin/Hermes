
#pragma once
#ifdef _WIN32
#include <WinSock2.h>

namespace Hermes {
    enum class SocketShutdownEnum : int {
        Receive = SD_RECEIVE,
        Send    = SD_SEND,
        Both    = SD_BOTH
    };
}
#else
#include <sys/socket.h>

namespace Hermes {
    enum class SocketShutdownEnum : int {
        Receive = SHUT_RD,
        Send    = SHUT_WR,
        Both    = SHUT_RDWR
    };
}
#endif