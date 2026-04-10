#pragma once
#include <sspi.h>

enum class AcceptSecurityContextReturnFlags : long long {
    Delegate             = ASC_RET_DELEGATE,
    MutualAuth           = ASC_RET_MUTUAL_AUTH,
    ReplayDetect         = ASC_RET_REPLAY_DETECT,
    SequenceDetect       = ASC_RET_SEQUENCE_DETECT,
    Confidentiality      = ASC_RET_CONFIDENTIALITY,
    UseSessionKey        = ASC_RET_USE_SESSION_KEY,
    SessionTicket        = ASC_RET_SESSION_TICKET,
    AllocateMemory       = ASC_RET_ALLOCATED_MEMORY,
    UseDceStyle          = ASC_RET_USED_DCE_STYLE,
    Datagram             = ASC_RET_DATAGRAM,
    Connection           = ASC_RET_CONNECTION,
    CallLevel            = ASC_RET_CALL_LEVEL, // skipped 1000 to be like ISC_
    FragmentSupplied     = ASC_RET_THIRD_LEG_FAILED,
    ExtendedError        = ASC_RET_EXTENDED_ERROR,
    Stream               = ASC_RET_STREAM,
    Integrity            = ASC_RET_INTEGRITY,
    Licensing            = ASC_RET_LICENSING,
    Identify             = ASC_RET_IDENTIFY,
    AllowNullSession     = ASC_RET_NULL_SESSION,
    AllowNonUserLogons   = ASC_RET_ALLOW_NON_USER_LOGONS,
    AllowContextReplay   = ASC_RET_ALLOW_CONTEXT_REPLAY, // deprecated - don't use this flag!!!
    FragmentToFit        = ASC_RET_FRAGMENT_ONLY,
    NoToken              = ASC_RET_NO_TOKEN,
    ProxyBindings        = ASC_RET_NO_ADDITIONAL_TOKEN, // *INTERNAL*
    AllowMissingBindings = ASC_RET_MESSAGES, // Indicates that the TLS 1.3+ record layer is disabled, and the security context consumes and produces cleartext TLS messages, rather than records.
    Messages             = ASC_RET_REUSE_SESSION_TICKETS, // Indicates that the TLS 1.3+ server will allow session ticket reuse.
    ExplicitSession      = ASC_RET_EXPLICIT_SESSION, // Indicates that explicit TLS 1.3+ session management is enabled.

};