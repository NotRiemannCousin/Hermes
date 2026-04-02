#pragma once
#include <ws2def.h>

enum class NameInfoFlags {
    NoFqdn      = NI_NOFQDN,      /* Only return nodename portion for local hosts */
    NumericHost = NI_NUMERICHOST, /* Return numeric form of the host's address */
    NameReqd    = NI_NAMEREQD,    /* Error if the host's name not in DNS */
    NumericServ = NI_NUMERICSERV, /* Return numeric form of the service (port #) */
    Dgram       = NI_DGRAM,       /* Service is a datagram service */

    MaxHost = NI_MAXHOST, /* Max size of a fully-qualified domain name */
    MaxServ = NI_MAXSERV, /* Max size of a service name */
};
