#pragma once
#include <winerror.h>

enum class ConditionFunctionEnum : int {
    ACCEPT = CF_ACCEPT,
    REJECT = CF_REJECT,
    DEFER  = CF_DEFER
};
