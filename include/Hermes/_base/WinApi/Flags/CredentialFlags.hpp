#pragma once
#include <winerror.h>

enum class CredentialFlags : unsigned long {
    INBOUND  = SECPKG_CRED_INBOUND,
    OUTBOUND = SECPKG_CRED_OUTBOUND,
    BOTH     = SECPKG_CRED_BOTH,
    DEFAULT  = SECPKG_CRED_DEFAULT,
    RESERVED = SECPKG_CRED_RESERVED
};