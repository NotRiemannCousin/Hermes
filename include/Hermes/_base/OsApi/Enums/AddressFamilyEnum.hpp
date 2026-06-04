#pragma once
#ifdef _WIN32
#include <ws2def.h>
namespace Hermes {
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
}
#else
#include <sys/socket.h>

namespace Hermes {
    enum class AddressFamilyEnum {
        Unspec    = AF_UNSPEC,

        Local     = AF_LOCAL,
        Unix      = AF_UNIX,
        File      = AF_FILE,

        Ipx       = AF_IPX,
        Appletalk = AF_APPLETALK,

        // Actual useful stuff
        Inet      = AF_INET,
        Inet6     = AF_INET6,
        Irda      = AF_IRDA,
        Bth       = AF_BLUETOOTH,

        Smc       = AF_SMC,
    };
}
#endif
