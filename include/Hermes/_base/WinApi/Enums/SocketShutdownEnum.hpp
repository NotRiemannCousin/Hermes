#pragma once
#include <WinSock2.h>

enum class SocketShutdownEnum : int {
    Receive = SD_RECEIVE,
    Send    = SD_SEND,
    Both    = SD_BOTH
};
