#pragma once
#include <WinSock2.h>

namespace Hermes {
    enum class ConditionFunctionEnum : int {
        Accept = CF_ACCEPT,
        Reject = CF_REJECT,
        Defer  = CF_DEFER
    };
}