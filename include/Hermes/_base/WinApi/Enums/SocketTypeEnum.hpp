#pragma once
#include <WinSock2.h>

enum class SocketTypeEnum : int {
    STREAM    = SOCK_STREAM,        /* stream socket */
    DGRAM     = SOCK_DGRAM,         /* datagram socket */
    RAW       = SOCK_RAW,           /* raw-protocol interface */
    RDM       = SOCK_RDM,           /* reliably-delivered message */
    SEQPACKET = SOCK_SEQPACKET,     /* sequenced packet stream */
};