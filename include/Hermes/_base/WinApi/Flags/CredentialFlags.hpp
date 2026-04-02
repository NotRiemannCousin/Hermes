#pragma once
#include <sspi.h>

enum class CredentialFlags : unsigned long {
    Inbound  = SECPKG_CRED_INBOUND,
    Outbound = SECPKG_CRED_OUTBOUND,
    Both     = SECPKG_CRED_BOTH,
    Default  = SECPKG_CRED_DEFAULT,
    Reserved = SECPKG_CRED_RESERVED
};