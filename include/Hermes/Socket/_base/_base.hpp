#pragma once
#include <ranges>

namespace Hermes {
    namespace rg = std::ranges;

    template<class Type>
    concept ByteLike = sizeof(Type) == 1;


    template<template<class> class T>
    concept MinimalInputSocketRangeConcept =
        std::ranges::input_range<T<std::byte>>;


    template<template<class> class T>
    concept MinimalOutputSocketRangeConcept =
        std::ranges::output_range<T<std::byte>, std::byte>;
}