#pragma once
#include <WinSock2.h>

enum class ConditionFunctionEnum : int {
    Accept = CF_ACCEPT,
    Reject = CF_REJECT,
    Defer  = CF_DEFER
};
