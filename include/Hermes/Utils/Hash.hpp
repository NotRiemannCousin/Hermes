#pragma once
#include <utility>
#include <functional>

namespace Hermes::Utils {
    //! @brief Checks whether a type can be hashed into a size value.
    template<typename T>
    concept Hashable = requires(T a) {
            { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
    };

    //! @brief Combines a hash value with an existing seed.
    //! @param seed The hash seed to update.
    //! @param v The hash value to mix into seed.
    //! @details This uses the Knuth / TEA hash combine approach, the same approach used by boost::hash_combine. The
    //! order of calls matters when composing fields.
    constexpr void HashCombine(size_t& seed, const size_t v) {
        seed ^= v + 0x9e3779b97f4a7c15ULL + (seed<<6) + (seed>>2);
    }
}
