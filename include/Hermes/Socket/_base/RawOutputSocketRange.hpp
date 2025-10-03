#pragma once
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base/_base.hpp>
#include <Hermes/_base/WinAPI.hpp>

#include <iterator>
#include <array>


namespace Hermes {
    template<class Type>
    struct RawOutputSocketRange {
        struct Iterator {
            using iterator_category = std::output_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = void;

            RawOutputSocketRange* view = nullptr;

            Iterator& operator*();
            Iterator& operator++();
            Iterator& operator++(int);
            Iterator& operator=(Type value);
        };

        explicit RawOutputSocketRange(SOCKET socket);
        ~RawOutputSocketRange();

        Iterator begin();
        std::default_sentinel_t end();

        ConnectionResultOper Flush();
        [[nodiscard]] ConnectionResultOper OptError() const;

    private:
        int index{};
        SOCKET _socket{};
        ConnectionResultOper errorStatus{};
        std::array<Type, 0x0F00> _buffer{};
    };

    template <class I, class T>
    concept Sla = requires(I __i, T&& __t) {
        *__i++ = static_cast<T&&>(__t);
    };

    static_assert(Sla<RawOutputSocketRange<std::byte>::Iterator, std::byte>);
}

#include <Hermes/Socket/_base/RawOutputSocketRange.tpp>
