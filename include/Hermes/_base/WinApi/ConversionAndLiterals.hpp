#pragma once
#include <stdint.h>


//! User-defined literal for uint8_t.
//! Usage: '123_uc' creates an uint8_t with value 123.
inline uint8_t operator""_uc(unsigned long long int n) {
    return static_cast<uint8_t>(n);
}


long long _tll(auto t) {
    return static_cast<long long>(t);
}

unsigned long long _tull(auto t) {
    return static_cast<unsigned long long>(t);
}

long _tl(auto t) {
    return static_cast<long>(t);
}

unsigned long _tul(auto t) {
    return static_cast<unsigned long>(t);
}


unsigned long _tus(auto t) {
    return static_cast<unsigned short>(t);
}
