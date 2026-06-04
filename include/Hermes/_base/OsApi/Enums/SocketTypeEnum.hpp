#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <sys/socket.h>
#endif

namespace Hermes {
    enum class SocketTypeEnum : int {
        Stream    = SOCK_STREAM,        /* stream socket */
        Dgram     = SOCK_DGRAM,         /* datagram socket */
        Raw       = SOCK_RAW,           /* raw-protocol interface */
        Rdm       = SOCK_RDM,           /* reliably-delivered message */
        SeqPacket = SOCK_SEQPACKET,     /* sequenced packet stream */
    };
}