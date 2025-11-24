#pragma once
#include <ws2def.h>

enum class ProtocolBaseTypeEnum : int {
    UDP = IPPROTO_UDP,
    TCP = IPPROTO_TCP
};