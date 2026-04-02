#pragma once
#include <schannel.h>

enum class SChCredEnum {
    V1              = SCH_CRED_V1,
    V2              = SCH_CRED_V2,
    V3              = SCH_CRED_V3,
    SChannel        = SCHANNEL_CRED_VERSION,
    SchCredentials = SCH_CREDENTIALS_VERSION,
};
