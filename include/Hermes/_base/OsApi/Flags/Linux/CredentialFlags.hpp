#pragma once

namespace Hermes {
    // OpenSSL credential direction. Picks the right SSL_method() in Credentials.cpp:
    //  - Inbound  -> TLS_server_method()
    //  - Outbound -> TLS_client_method()
    //  - Both     -> TLS_method()
    enum class CredentialFlags : unsigned long {
        Inbound  = 0x1,
        Outbound = 0x2,
        Both     = Inbound | Outbound,
    };
}