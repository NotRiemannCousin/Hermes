#pragma once
#include <span>
#include <Hermes/Socket/_base.hpp>

namespace Hermes::Utils {
    //! @brief Views a byte-like range as a read-only byte span.
    //! @param r A contiguous, viewable range whose values satisfy ByteLike.
    //! @return A span over the same storage, reinterpreted as `std::byte`.
    //! @details The returned span does not own the data. The range must remain alive and its storage must not be
    //! invalidated while the span is being used.
    template<std::ranges::viewable_range R>
        requires ByteLike<std::ranges::range_value_t<R>>
    std::span<const std::byte> ToBytes(const R& r) {
        return std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(std::ranges::data(r)),
            std::ranges::size(r)
        );
    }
}
