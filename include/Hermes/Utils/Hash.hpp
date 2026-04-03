#pragma once
#include <utility>

namespace Hermes::Utils {
    template<typename T>
    concept Hashable = requires(T a) {
            { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
    };

    //! Knuth / TEA hash combine (same approach used by boost::hash_combine).
    constexpr void HashCombine(size_t& seed, const size_t v) {
        seed ^= v + 0x9e3779b97f4a7c15ULL + (seed<<6) + (seed>>2);
    }
}
