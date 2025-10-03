#pragma once
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base/_base.hpp>
#include <Hermes/_base/WinAPI.hpp>

#include <chrono>
#include <array>


namespace Hermes {
    template<ByteLike Type>
    struct RawInputSocketRange {
        struct Iterator {
            using difference_type  = std::ptrdiff_t;
            using value_type       = Type;

            RawInputSocketRange* view = nullptr;

            [[nodiscard]] value_type operator*() const;
            Iterator& operator++();
            Iterator& operator++(int);
            [[nodiscard]] bool operator==(std::default_sentinel_t) const;
        };

        explicit RawInputSocketRange(SOCKET socket);

        Iterator begin();
        std::default_sentinel_t end();

        ConnectionResultOper OptError();
    private:
        ConnectionResultOper Receive();

        static constexpr size_t bufferSize{ 0x0F00 };

        int _index{};
        int _size{};
        SOCKET _socket{};
        ConnectionResultOper _errorStatus{};
        std::array<Type, 0x0F00> _buffer{};
    };


    static_assert(MinimalInputSocketRangeConcept<RawInputSocketRange>);
}

#include <Hermes/Socket/_base/RawInputSocketRange.tpp>
