#pragma once
#include <schannel.h>

namespace Hermes {
    enum class SChCredEnum {
        V1              = SCH_CRED_V1,
        V2              = SCH_CRED_V2,
        V3              = SCH_CRED_V3,
        SChannel        = SCHANNEL_CRED_VERSION,
        SchCredentials = SCH_CREDENTIALS_VERSION,
    };
}