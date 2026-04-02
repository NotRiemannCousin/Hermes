#pragma once
#include <sspi.h>

enum class SecurityBufferEnum : long long {
    Empty                        = SECBUFFER_EMPTY,            // Undefined, replaced by provider
    Data                         = SECBUFFER_DATA,             // Packet data
    Token                        = SECBUFFER_TOKEN,            // Security token
    PkgParams                    = SECBUFFER_PKG_PARAMS,       // Package specific parameters
    Missing                      = SECBUFFER_MISSING,          // Missing Data indicator
    Extra                        = SECBUFFER_EXTRA,            // Extra data
    StreamTrailer                = SECBUFFER_STREAM_TRAILER,   // Security Trailer
    StreamHeader                 = SECBUFFER_STREAM_HEADER,    // Security Header
    NegotiationInfo              = SECBUFFER_NEGOTIATION_INFO, // Hints from the negotiation pkg
    Padding                      = SECBUFFER_PADDING,          // non-data padding
    Stream                       = SECBUFFER_STREAM,           // whole encrypted message
    MechList                     = SECBUFFER_MECHLIST,
    MechListSignature            = SECBUFFER_MECHLIST_SIGNATURE,
    Target                       = SECBUFFER_TARGET, // obsolete
    ChannelBindings              = SECBUFFER_CHANNEL_BINDINGS,
    ChangePassResponse           = SECBUFFER_CHANGE_PASS_RESPONSE,
    TargetHost                   = SECBUFFER_TARGET_HOST,
    Alert                        = SECBUFFER_ALERT,
    ApplicationProtocols         = SECBUFFER_APPLICATION_PROTOCOLS,    // Lists of application protocol IDs, one per negotiation extension
    SrtpProtectionProfiles       = SECBUFFER_SRTP_PROTECTION_PROFILES, // List of SRTP protection profiles, in descending order of preference
    SrtpMasterKeyIdentifier      = SECBUFFER_SRTP_MASTER_KEY_IDENTIFIER,      // SRTP master key identifier
    TokenBinding                 = SECBUFFER_TOKEN_BINDING,                   // Supported Token Binding protocol version and key parameters
    PresharedKey                 = SECBUFFER_PRESHARED_KEY,                   // Preshared key
    PresharedKeyIdentity         = SECBUFFER_PRESHARED_KEY_IDENTITY,          // Preshared key identity
    DtlsMtu                      = SECBUFFER_DTLS_MTU,                        // DTLS path MTU setting
    SendGenericTlsExtension      = SECBUFFER_SEND_GENERIC_TLS_EXTENSION,      // Buffer for sending generic TLS extensions.
    SubscribeGenericTlsExtension = SECBUFFER_SUBSCRIBE_GENERIC_TLS_EXTENSION, // Buffer for subscribing to generic TLS extensions.
    Flags                        = SECBUFFER_FLAGS,                           // ISC/ASC REQ Flags
    TrafficSecrets               = SECBUFFER_TRAFFIC_SECRETS,                 // Message sequence lengths and corresponding traffic secrets.
    CertificateRequestContext    = SECBUFFER_CERTIFICATE_REQUEST_CONTEXT,     // TLS 1.3 certificate request context.

    Attrmask             = SECBUFFER_ATTRMASK,
    Readonly             = SECBUFFER_READONLY,              // Buffer is read-only, no checksum
    ReadonlyWithChecksum = SECBUFFER_READONLY_WITH_CHECKSUM,// Buffer is read-only, and checksummed
    Reserved             = SECBUFFER_RESERVED,              // Flags reserved to security system
};