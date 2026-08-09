#pragma once

namespace Hermes {
    // TLS/DTLS protocol selection. On OpenSSL these are applied via
    // SSL_CTX_set_min_proto_version / SSL_CTX_set_max_proto_version
    // (TLS1_2_VERSION, TLS1_3_VERSION, DTLS1_2_VERSION, ...).
    // Legacy SSL2/SSL3/TLS1.0/TLS1.1 are intentionally omitted — OpenSSL 3.x
    // disables them by default and we follow the same policy.
    enum class SupportedProtocolsFlags : long long {
        None  = 0,

        Tls12 = 1ll << 0,
        Tls13 = 1ll << 1,

        Dtls12 = 1ll << 2,

        All = Tls12 | Tls13 | Dtls12,
    };
}