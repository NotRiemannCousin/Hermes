#pragma once
#include <ws2def.h>

enum class NameInfoFlags {
    NOFQDN      = NI_NOFQDN,      /* Only return nodename portion for local hosts */
    NUMERICHOST = NI_NUMERICHOST, /* Return numeric form of the host's address */
    NAMEREQD    = NI_NAMEREQD,    /* Error if the host's name not in DNS */
    NUMERICSERV = NI_NUMERICSERV, /* Return numeric form of the service (port #) */
    DGRAM       = NI_DGRAM,       /* Service is a datagram service */

    MAXHOST = NI_MAXHOST, /* Max size of a fully-qualified domain name */
    MAXSERV = NI_MAXSERV, /* Max size of a service name */
};
