#pragma once
#include <winerror.h>

enum class SocketShutdownEnum : int {
    RECEIVE = SD_RECEIVE,
    SEND = SD_SEND,
    BOTH = SD_BOTH
};
