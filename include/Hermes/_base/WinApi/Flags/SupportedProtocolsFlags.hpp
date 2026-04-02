#pragma once
#include <schannel.h>

enum class SupportedProtocolsFlags : long long {
    Pct1Server = SP_PROT_PCT1_SERVER,
    Pct1Client = SP_PROT_PCT1_CLIENT,
    Pct1       = SP_PROT_PCT1,

    Ssl2Server = SP_PROT_SSL2_SERVER,
    Ssl2Client = SP_PROT_SSL2_CLIENT,
    Ssl2       = SP_PROT_SSL2,

    Ssl3Server = SP_PROT_SSL3_SERVER,
    Ssl3Client = SP_PROT_SSL3_CLIENT,
    Ssl3       = SP_PROT_SSL3,

    Tls1Server = SP_PROT_TLS1_SERVER,
    Tls1Client = SP_PROT_TLS1_CLIENT,
    Tls1       = SP_PROT_TLS1,

    Ssl3Tls1Clients = SP_PROT_SSL3TLS1_CLIENTS,
    Ssl3Tls1Servers = SP_PROT_SSL3TLS1_SERVERS,
    Ssl3Tls1        = SP_PROT_SSL3TLS1,

    UniServer = SP_PROT_UNI_SERVER,
    UniClient = SP_PROT_UNI_CLIENT,
    Uni       = SP_PROT_UNI,

    All     = SP_PROT_ALL,
    None    = SP_PROT_NONE,
    Clients = SP_PROT_CLIENTS,
    Servers = SP_PROT_SERVERS,


    Tls10Server = SP_PROT_TLS1_0_SERVER,
    Tls10Client = SP_PROT_TLS1_0_CLIENT,
    Tls10       = SP_PROT_TLS1_0,

    Tls11Server = SP_PROT_TLS1_1_SERVER,
    Tls11Client = SP_PROT_TLS1_1_CLIENT,
    Tls11       = SP_PROT_TLS1_1,

    Tls12Server = SP_PROT_TLS1_2_SERVER,
    Tls12Client = SP_PROT_TLS1_2_CLIENT,
    Tls12       = SP_PROT_TLS1_2,

    Tls13Server = SP_PROT_TLS1_3_SERVER,
    Tls13Client = SP_PROT_TLS1_3_CLIENT,
    Tls13       = SP_PROT_TLS1_3,

    DtlsServer  = SP_PROT_DTLS_SERVER,
    DtlsClient  = SP_PROT_DTLS_CLIENT,
    Dtls        = SP_PROT_DTLS,

    Dtls1_0Server = SP_PROT_DTLS1_0_SERVER,
    Dtls1_0Client = SP_PROT_DTLS1_0_CLIENT,
    Dtls1_0       = SP_PROT_DTLS1_0,

    Dtls1_2Server = SP_PROT_DTLS1_2_SERVER,
    Dtls1_2Client = SP_PROT_DTLS1_2_CLIENT,
    Dtls1_2       = SP_PROT_DTLS1_2,

    Dtls1XServer = SP_PROT_DTLS1_X_SERVER,

    Dtls1XClient = SP_PROT_DTLS1_X_CLIENT,

    Dtls1X = SP_PROT_DTLS1_X,

    Tls11PlusServer = SP_PROT_TLS1_1PLUS_SERVER,
    Tls11PlusClient = SP_PROT_TLS1_1PLUS_CLIENT,

    Tls11Plus = SP_PROT_TLS1_1PLUS,

    Tls13PlusServer = SP_PROT_TLS1_3PLUS_SERVER,
    Tls13PlusClient = SP_PROT_TLS1_3PLUS_CLIENT,
    Tls13Plus       = SP_PROT_TLS1_3PLUS,

    Tls1XServer = SP_PROT_TLS1_X_SERVER,
    Tls1XClient = SP_PROT_TLS1_X_CLIENT,
    Tls1X       = SP_PROT_TLS1_X,

    Ssl3Tls1XClients = SP_PROT_SSL3TLS1_X_CLIENTS,
    Ssl3Tls1XServers = SP_PROT_SSL3TLS1_X_SERVERS,
    Ssl3Tls1X        = SP_PROT_SSL3TLS1_X,

    XClients = SP_PROT_X_CLIENTS,
    XServers = SP_PROT_X_SERVERS
};
