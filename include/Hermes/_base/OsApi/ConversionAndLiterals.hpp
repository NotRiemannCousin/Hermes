#pragma once
#include <cstdint>


//! User-defined literal for uint8_t.
//! Usage: '123_uc' creates an uint8_t with value 123.
inline uint8_t operator""_uc(unsigned long long int n) {
    return static_cast<uint8_t>(n);
}


inline long long tll(auto t) {
    return static_cast<long long>(t);
}

inline unsigned long long tull(auto t) {
    return static_cast<unsigned long long>(t);
}

inline long tl(auto t) {
    return static_cast<long>(t);
}

inline unsigned long tul(auto t) {
    return static_cast<unsigned long>(t);
}


inline unsigned long tus(auto t) {
    return static_cast<unsigned short>(t);
}

