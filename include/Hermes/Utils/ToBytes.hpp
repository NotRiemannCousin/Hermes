#pragma once
#include <span>
#include <Hermes/Socket/_base/_base.hpp>

namespace Hermes::Utils {
    template<std::ranges::viewable_range R>
        requires ByteLike<std::ranges::range_value_t<R>>
    std::span<const std::byte> ToBytes(const R& r) {
        return std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(std::ranges::data(r)),
            std::ranges::size(r)
        );
    }
}
