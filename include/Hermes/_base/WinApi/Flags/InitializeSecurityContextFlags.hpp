#pragma once
#include <sspi.h>

enum class InitializeSecurityContextFlags : long long {
    DELEGATE               = ISC_REQ_DELEGATE,
    MUTUAL_AUTH            = ISC_REQ_MUTUAL_AUTH,
    REPLAY_DETECT          = ISC_REQ_REPLAY_DETECT,
    SEQUENCE_DETECT        = ISC_REQ_SEQUENCE_DETECT,
    CONFIDENTIALITY        = ISC_REQ_CONFIDENTIALITY,
    USE_SESSION_KEY        = ISC_REQ_USE_SESSION_KEY,
    PROMPT_FOR_CREDS       = ISC_REQ_PROMPT_FOR_CREDS,
    USE_SUPPLIED_CREDS     = ISC_REQ_USE_SUPPLIED_CREDS,
    ALLOCATE_MEMORY        = ISC_REQ_ALLOCATE_MEMORY,
    USE_DCE_STYLE          = ISC_REQ_USE_DCE_STYLE,
    DATAGRAM               = ISC_REQ_DATAGRAM,
    CONNECTION             = ISC_REQ_CONNECTION,
    CALL_LEVEL             = ISC_REQ_CALL_LEVEL,
    FRAGMENT_SUPPLIED      = ISC_REQ_FRAGMENT_SUPPLIED,
    EXTENDED_ERROR         = ISC_REQ_EXTENDED_ERROR,
    STREAM                 = ISC_REQ_STREAM,
    INTEGRITY              = ISC_REQ_INTEGRITY,
    IDENTIFY               = ISC_REQ_IDENTIFY,
    NULL_SESSION           = ISC_REQ_NULL_SESSION,
    MANUAL_CRED_VALIDATION = ISC_REQ_MANUAL_CRED_VALIDATION,
    RESERVED1              = ISC_REQ_RESERVED1,
    FRAGMENT_TO_FIT        = ISC_REQ_FRAGMENT_TO_FIT,
    // This exists only in Windows Vista and greater
    FORWARD_CREDENTIALS    = ISC_REQ_FORWARD_CREDENTIALS,
    NO_INTEGRITY           = ISC_REQ_NO_INTEGRITY, // honored only by SPNEGO
    USE_HTTP_STYLE         = ISC_REQ_USE_HTTP_STYLE,
    UNVERIFIED_TARGET_NAME = ISC_REQ_UNVERIFIED_TARGET_NAME,
    CONFIDENTIALITY_ONLY   = ISC_REQ_CONFIDENTIALITY_ONLY, // honored by SPNEGO/Kerberos
    MESSAGES               = ISC_REQ_MESSAGES, // Disables the TLS 1.3+ record layer and causes the security context to
                                     // consume and produce cleartext TLS messages, rather than records.
    // Request that schannel perform server cert chain validation without failing the handshake on errors
    // (deferred), same as SCH_CRED_DEFERRED_CRED_VALIDATION except applies to context not credential handle.
    DEFERRED_CRED_VALIDATION = ISC_REQ_DEFERRED_CRED_VALIDATION,
    // Prevents the client sending the post_handshake_auth extension in the TLS 1.3 Client Hello.
    NO_POST_HANDSHAKE_AUTH = ISC_REQ_NO_POST_HANDSHAKE_AUTH,

};