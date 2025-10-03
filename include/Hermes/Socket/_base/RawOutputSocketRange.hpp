#pragma once
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base/_base.hpp>
#include <Hermes/_base/WinAPI.hpp>

#include <iterator>
#include <array>


namespace Hermes {
    struct RawOutputSocketRange {
        struct Iterator {
            using iterator_category = std::output_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = void;

            RawOutputSocketRange* view = nullptr;

            Iterator& operator*();
            Iterator& operator++();
            Iterator& operator++(int);

            template<ByteLike Type>
            Iterator& operator=(Type value);
            [[nodiscard]] bool operator==(std::default_sentinel_t) const;
        };

        explicit RawOutputSocketRange(SOCKET socket);
        ~RawOutputSocketRange();

        Iterator begin();
        std::default_sentinel_t end();

        ConnectionResultOper Flush();
        [[nodiscard]] ConnectionResultOper OptError() const;

    private:
        static constexpr size_t bufferSize{ 0x0F00 };

        int index{};
        SOCKET _socket{};
        ConnectionResultOper _errorStatus{};
        std::array<std::byte, 0x0F00> _buffer{};
    };


    static_assert(MinimalOutputSocketRangeConcept<RawOutputSocketRange>);
}

#include <Hermes/Socket/_base/RawOutputSocketRange.tpp>
