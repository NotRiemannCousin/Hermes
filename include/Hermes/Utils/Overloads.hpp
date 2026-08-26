#pragma once

namespace Hermes::Utils {
    // thanks https://en.cppreference.com/w/cpp/utility/variant/visit2.html
    //! @brief Combines several function objects into one overloaded function object.
    //! @details This is mainly useful with std::visit, where each lambda handles one alternative of a variant. The
    //! helper inherits every callable and exposes all their operator() overloads.
    template<class... Ts>
    struct Overloaded : Ts... { using Ts::operator()...; };
}