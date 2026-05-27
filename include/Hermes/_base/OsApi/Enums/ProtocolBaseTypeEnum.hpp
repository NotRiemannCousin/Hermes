#pragma once
#ifdef _WIN32
#include <ws2def.h>
#else
#include <netinet/in.h>
#endif

enum class ProtocolBaseTypeEnum : int {
    Udp = IPPROTO_UDP,
    Tcp = IPPROTO_TCP
};