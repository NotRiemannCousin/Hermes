#pragma once
#include <schannel.h>

enum class SchannelCredFlags {
    NO_DEFAULT_CREDS       = SCH_CRED_NO_DEFAULT_CREDS,
    MANUAL_CRED_VALIDATION = SCH_CRED_MANUAL_CRED_VALIDATION
};
