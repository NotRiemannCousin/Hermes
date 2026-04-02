#pragma once
#include <ws2def.h>

enum class AddressFamilyEnum {
    Unspec    = AF_UNSPEC,

    Ipx       = AF_IPX,
    Appletalk = AF_APPLETALK,
    Netbios   = AF_NETBIOS,

    // Actual useful stuff
    Inet      = AF_INET,
    Inet6     = AF_INET6,
    Irda      = AF_IRDA,
    Bth       = AF_BTH,
};
