#pragma once
#include <sspi.h>

enum class InitializeSecurityContextReturnFlags : long long {
    Delegate             = ISC_RET_DELEGATE,
    MutualAuth           = ISC_RET_MUTUAL_AUTH,
    ReplayDetect         = ISC_RET_REPLAY_DETECT,
    SequenceDetect       = ISC_RET_SEQUENCE_DETECT,
    Confidentiality      = ISC_RET_CONFIDENTIALITY,
    UseSessionKey        = ISC_RET_USE_SESSION_KEY,
    UsedCollectedCreds   = ISC_RET_USED_COLLECTED_CREDS,
    UsedSuppliedCreds    = ISC_RET_USED_SUPPLIED_CREDS,
    AllocatedMemory      = ISC_RET_ALLOCATED_MEMORY,
    UsedDceStyle         = ISC_RET_USED_DCE_STYLE,
    Datagram             = ISC_RET_DATAGRAM,
    Connection           = ISC_RET_CONNECTION,
    IntermediateReturn   = ISC_RET_INTERMEDIATE_RETURN,
    CallLevel            = ISC_RET_CALL_LEVEL,
    ExtendedError        = ISC_RET_EXTENDED_ERROR,
    Stream               = ISC_RET_STREAM,
    Integrity            = ISC_RET_INTEGRITY,
    Identify             = ISC_RET_IDENTIFY,
    NullSession          = ISC_RET_NULL_SESSION,
    ManualCredValidation = ISC_RET_MANUAL_CRED_VALIDATION,
    Reserved1            = ISC_RET_RESERVED1,
    FragmentOnly         = ISC_RET_FRAGMENT_ONLY,
    // This exists only in Windows Vista and greater
    ForwardCredentials   = ISC_RET_FORWARD_CREDENTIALS,

    UsedHttpStyle          = ISC_RET_USED_HTTP_STYLE,
    NoAdditionalToken      = ISC_RET_NO_ADDITIONAL_TOKEN,
    Reauthentication       = ISC_RET_REAUTHENTICATION,
    ConfidentialityOnly    = ISC_RET_CONFIDENTIALITY_ONLY,
    Messages               = ISC_RET_MESSAGES,
    DeferredCredValidation = ISC_RET_DEFERRED_CRED_VALIDATION,
    NoPostHandshakeAuth    = ISC_RET_NO_POST_HANDSHAKE_AUTH,
};
