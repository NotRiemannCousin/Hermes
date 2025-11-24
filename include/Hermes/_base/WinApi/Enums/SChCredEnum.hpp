#pragma once
#include <winerror.h>

enum class SChCredEnum {
    V1              = SCH_CRED_V1,
    V2              = SCH_CRED_V2,
    V3              = SCH_CRED_V3,
    SCHANNEL        = SCHANNEL_CRED_VERSION,
    SCH_CREDENTIALS = SCH_CREDENTIALS_VERSION,
};
