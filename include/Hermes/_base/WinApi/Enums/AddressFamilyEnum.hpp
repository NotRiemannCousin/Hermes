#pragma once
#include <ws2def.h>

enum class AddressFamilyEnum {
    UNSPEC    = AF_UNSPEC,

    IPX       = AF_IPX,
    APPLETALK = AF_APPLETALK,
    NETBIOS   = AF_NETBIOS,

    // Actual useful stuff
    INET      = AF_INET,
    INET6     = AF_INET6,
    IRDA      = AF_IRDA,
    BTH       = AF_BTH,
};
