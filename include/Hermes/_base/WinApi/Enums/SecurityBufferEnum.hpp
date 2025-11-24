#pragma once
#include <winerror.h>

enum class SecurityBufferEnum : long long {
    EMPTY                    = SECBUFFER_EMPTY,            // Undefined, replaced by provider
    DATA                     = SECBUFFER_DATA,             // Packet data
    TOKEN                    = SECBUFFER_TOKEN,            // Security token
    PKG_PARAMS               = SECBUFFER_PKG_PARAMS,       // Package specific parameters
    MISSING                  = SECBUFFER_MISSING,          // Missing Data indicator
    EXTRA                    = SECBUFFER_EXTRA,            // Extra data
    STREAM_TRAILER           = SECBUFFER_STREAM_TRAILER,   // Security Trailer
    STREAM_HEADER            = SECBUFFER_STREAM_HEADER,    // Security Header
    NEGOTIATION_INFO         = SECBUFFER_NEGOTIATION_INFO, // Hints from the negotiation pkg
    PADDING                  = SECBUFFER_PADDING,          // non-data padding
    STREAM                   = SECBUFFER_STREAM,           // whole encrypted message
    MECHLIST                 = SECBUFFER_MECHLIST,
    MECHLIST_SIGNATURE       = SECBUFFER_MECHLIST_SIGNATURE,
    TARGET                   = SECBUFFER_TARGET, // obsolete
    CHANNEL_BINDINGS         = SECBUFFER_CHANNEL_BINDINGS,
    CHANGE_PASS_RESPONSE     = SECBUFFER_CHANGE_PASS_RESPONSE,
    TARGET_HOST              = SECBUFFER_TARGET_HOST,
    ALERT                    = SECBUFFER_ALERT,
    APPLICATION_PROTOCOLS    = SECBUFFER_APPLICATION_PROTOCOLS,    // Lists of application protocol IDs, one per negotiation extension
    SRTP_PROTECTION_PROFILES = SECBUFFER_SRTP_PROTECTION_PROFILES, // List of SRTP protection profiles, in
    // descending order of preference
    SRTP_MASTER_KEY_IDENTIFIER      = SECBUFFER_SRTP_MASTER_KEY_IDENTIFIER,      // SRTP master key identifier
    TOKEN_BINDING                   = SECBUFFER_TOKEN_BINDING,                   // Supported Token Binding protocol version and key parameters
    PRESHARED_KEY                   = SECBUFFER_PRESHARED_KEY,                   // Preshared key
    PRESHARED_KEY_IDENTITY          = SECBUFFER_PRESHARED_KEY_IDENTITY,          // Preshared key identity
    DTLS_MTU                        = SECBUFFER_DTLS_MTU,                        // DTLS path MTU setting
    SEND_GENERIC_TLS_EXTENSION      = SECBUFFER_SEND_GENERIC_TLS_EXTENSION,      // Buffer for sending generic TLS extensions.
    SUBSCRIBE_GENERIC_TLS_EXTENSION = SECBUFFER_SUBSCRIBE_GENERIC_TLS_EXTENSION, // Buffer for subscribing to generic TLS extensions.
    FLAGS                           = SECBUFFER_FLAGS,                           // ISC/ASC REQ Flags
    TRAFFIC_SECRETS                 = SECBUFFER_TRAFFIC_SECRETS,                 // Message sequence lengths and corresponding traffic secrets.
    CERTIFICATE_REQUEST_CONTEXT     = SECBUFFER_CERTIFICATE_REQUEST_CONTEXT,     // TLS 1.3 certificate request context.

    ATTRMASK               = SECBUFFER_ATTRMASK,
    READONLY               = SECBUFFER_READONLY,              // Buffer is read-only, no checksum
    READONLY_WITH_CHECKSUM = SECBUFFER_READONLY_WITH_CHECKSUM,// Buffer is read-only, and checksummed
    RESERVED               = SECBUFFER_RESERVED,              // Flags reserved to security system
};