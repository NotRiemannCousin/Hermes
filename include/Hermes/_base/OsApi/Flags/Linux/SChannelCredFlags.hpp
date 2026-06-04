#pragma once

namespace Hermes {
    // Subset of SChannel flags adapted to OpenSSL behavior. Only flags with a
    // meaningful OpenSSL counterpart are kept; the rest were Windows-only knobs
    // (V1/V2/V3, CAPI/X509 chain formats, SNI cred bits) that don't translate.
    enum class SChannelCredFlags : unsigned long {
        None                    = 0,

        // Peer verification (maps to SSL_CTX_set_verify mode)
        AutoCredValidation      = 1u << 0, // SSL_VERIFY_PEER
        ManualCredValidation    = 1u << 1, // SSL_VERIFY_NONE (app validates)
        NoServernameCheck       = 1u << 2, // Skip X509_VERIFY_PARAM hostname check

        // Default trust store (system CAs via SSL_CTX_set_default_verify_paths)
        UseDefaultCreds         = 1u << 3,
        NoDefaultCreds          = 1u << 4,

        // Session resumption (SSL_OP_NO_TICKET / SSL_CTX_set_session_cache_mode)
        DisableReconnects       = 1u << 5,

        // CRL / revocation (X509_V_FLAG_CRL_CHECK family)
        RevocationCheckEndCert  = 1u << 6,
        RevocationCheckChain    = 1u << 7,
        IgnoreNoRevocationCheck = 1u << 8,
        IgnoreRevocationOffline = 1u << 9,
    };
}