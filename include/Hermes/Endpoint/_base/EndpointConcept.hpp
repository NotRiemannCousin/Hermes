#pragma once
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/_base/WinAPI.hpp>

namespace Hermes {
    using SocketInfoAddr = std::tuple<sockaddr, size_t, AddressFamilyEnum>;

    template<typename T>
    concept EndpointConcept = requires(T t, SocketInfoAddr socketInfo) {
        { t.FromSockAddr(socketInfo) } ->std::same_as<ConnectionResultOper>;
        { t.ToSockAddr() } ->std::same_as<ConnectionResult<SocketInfoAddr>>;
    };
} // namespace Hermes