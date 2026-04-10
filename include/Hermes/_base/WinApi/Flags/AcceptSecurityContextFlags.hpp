#pragma once
#include <sspi.h>

enum class AcceptSecurityContextFlags : long long {
    Delegate             = ASC_REQ_DELEGATE,
    MutualAuth           = ASC_REQ_MUTUAL_AUTH,
    ReplayDetect         = ASC_REQ_REPLAY_DETECT,
    SequenceDetect       = ASC_REQ_SEQUENCE_DETECT,
    Confidentiality      = ASC_REQ_CONFIDENTIALITY,
    UseSessionKey        = ASC_REQ_USE_SESSION_KEY,
    SessionTicket        = ASC_REQ_SESSION_TICKET,
    AllocateMemory       = ASC_REQ_ALLOCATE_MEMORY,
    UseDceStyle          = ASC_REQ_USE_DCE_STYLE,
    Datagram             = ASC_REQ_DATAGRAM,
    Connection           = ASC_REQ_CONNECTION,
    CallLevel            = ASC_REQ_CALL_LEVEL,
    FragmentSupplied     = ASC_REQ_FRAGMENT_SUPPLIED,
    ExtendedError        = ASC_REQ_EXTENDED_ERROR,
    Stream               = ASC_REQ_STREAM,
    Integrity            = ASC_REQ_INTEGRITY,
    Licensing            = ASC_REQ_LICENSING,
    Identify             = ASC_REQ_IDENTIFY,
    AllowNullSession     = ASC_REQ_ALLOW_NULL_SESSION,
    AllowNonUserLogons   = ASC_REQ_ALLOW_NON_USER_LOGONS,
    AllowContextReplay   = ASC_REQ_ALLOW_CONTEXT_REPLAY,
    FragmentToFit        = ASC_REQ_FRAGMENT_TO_FIT,

    NoToken              = ASC_REQ_NO_TOKEN,
    ProxyBindings        = ASC_REQ_PROXY_BINDINGS,
    AllowMissingBindings = ASC_REQ_ALLOW_MISSING_BINDINGS,
    Messages             = ASC_REQ_MESSAGES, // Disables the TLS 1.3+ record layer and causes the security context to consume and produce cleartext TLS messages, rather than records.
    // Request explicit TLS 1.3+ session management. TLS 1.3+ session ticket will only be generated when requested by the SSPI caller.
    ExplicitSession      = ASC_REQ_EXPLICIT_SESSION,
};