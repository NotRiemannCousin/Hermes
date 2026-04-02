#pragma once
#include <schannel.h>

enum class SChannelCredFlags {
    NoDefaultCreds       = SCH_CRED_NO_DEFAULT_CREDS,
    ManualCredValidation = SCH_CRED_MANUAL_CRED_VALIDATION
};
