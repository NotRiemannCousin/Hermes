#pragma once
#include <ws2def.h>

enum class ProtocolBaseTypeEnum : int {
    Udp = IPPROTO_UDP,
    Tcp = IPPROTO_TCP
};