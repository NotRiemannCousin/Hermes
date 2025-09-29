#pragma once
#include <chrono>
#include <array>
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/_base/WinAPI.hpp>


namespace Hermes {

    template<template<class> class T>
    concept InputSocketViewConcept =
        std::ranges::input_range<T<std::byte>>
        && std::ranges::input_range<T<char>>;

    template<class Type>
    struct RawInputSocketView {
        struct Iterator {
            using difference_type  = std::ptrdiff_t;
            using value_type       = Type;

            RawInputSocketView* view = nullptr;

            [[nodiscard]] value_type operator*() const;
            Iterator& operator++();
            void operator++(int);
            [[nodiscard]] bool operator==(std::default_sentinel_t) const;
        };

        RawInputSocketView(SOCKET socket);

        Iterator begin();
        std::default_sentinel_t end();

        ConnectionResultOper OptError();
    private:
        ConnectionResultOper Receive();

        int index{};
        int size{};
        SOCKET _socket{};
        ConnectionResultOper errorStatus{};
        std::array<Type, 0x0F00> _buffer{};
    };


    static_assert(InputSocketViewConcept<RawInputSocketView>);
}

#include <Hermes/Socket/_base/RawInputSocketView.tpp>
