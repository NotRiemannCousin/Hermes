#pragma once
#include <sspi.h>

enum class InitializeSecurityContextFlags : long long {
    Delegate             = ISC_REQ_DELEGATE,
    MutualAuth           = ISC_REQ_MUTUAL_AUTH,
    ReplayDetect         = ISC_REQ_REPLAY_DETECT,
    SequenceDetect       = ISC_REQ_SEQUENCE_DETECT,
    Confidentiality      = ISC_REQ_CONFIDENTIALITY,
    UseSessionKey        = ISC_REQ_USE_SESSION_KEY,
    PromptForCreds       = ISC_REQ_PROMPT_FOR_CREDS,
    UseSuppliedCreds     = ISC_REQ_USE_SUPPLIED_CREDS,
    AllocateMemory       = ISC_REQ_ALLOCATE_MEMORY,
    UseDceStyle          = ISC_REQ_USE_DCE_STYLE,
    Datagram             = ISC_REQ_DATAGRAM,
    Connection           = ISC_REQ_CONNECTION,
    CallLevel            = ISC_REQ_CALL_LEVEL,
    FragmentSupplied     = ISC_REQ_FRAGMENT_SUPPLIED,
    ExtendedError        = ISC_REQ_EXTENDED_ERROR,
    Stream               = ISC_REQ_STREAM,
    Integrity            = ISC_REQ_INTEGRITY,
    Identify             = ISC_REQ_IDENTIFY,
    NullSession          = ISC_REQ_NULL_SESSION,
    ManualCredValidation = ISC_REQ_MANUAL_CRED_VALIDATION,
    Reserved1            = ISC_REQ_RESERVED1,
    FragmentToFit        = ISC_REQ_FRAGMENT_TO_FIT,
    // This exists only in Windows Vista and greater
    ForwardCredentials   = ISC_REQ_FORWARD_CREDENTIALS,
    NoIntegrity          = ISC_REQ_NO_INTEGRITY, // honored only by SPNEGO
    UseHttpStyle         = ISC_REQ_USE_HTTP_STYLE,
    UnverifiedTargetName = ISC_REQ_UNVERIFIED_TARGET_NAME,
    ConfidentialityOnly  = ISC_REQ_CONFIDENTIALITY_ONLY, // honored by SPNEGO/Kerberos
    Messages             = ISC_REQ_MESSAGES, // Disables the TLS 1.3+ record layer and causes the security context to
                                     // consume and produce cleartext TLS messages, rather than records.
    // Request that schannel perform server cert chain validation without failing the handshake on errors
    // (deferred), same as SCH_CRED_DEFERRED_CRED_VALIDATION except applies to context not credential handle.
    DeferredCredValidation = ISC_REQ_DEFERRED_CRED_VALIDATION,
    // Prevents the client sending the post_handshake_auth extension in the TLS 1.3 Client Hello.
    NoPostHandshakeAuth = ISC_REQ_NO_POST_HANDSHAKE_AUTH,

};