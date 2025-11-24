#pragma once
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>

#include <Hermes/Socket/_base/_base.hpp>

namespace Hermes {
    template<
            EndpointConcept Endpoint = IpEndpoint,
            SocketTypeEnum SocketType = SocketTypeEnum::STREAM,
            AddressFamilyEnum SocketFamily = AddressFamilyEnum::INET6>
    struct DefaultSocketData {
        using EndpointType = Endpoint;

        static constexpr SocketTypeEnum Type = SocketType;
        static constexpr AddressFamilyEnum Family = SocketFamily;

        DefaultSocketData() = default;
        // ReSharper disable once CppFunctionIsNotImplemented
        DefaultSocketData(Endpoint&& other) noexcept;
        ~DefaultSocketData() = default;

        DefaultSocketData(DefaultSocketData&& other) noexcept;
        DefaultSocketData& operator=(DefaultSocketData&& other) noexcept;

        Endpoint endpoint{};
        SOCKET   socket{ macroINVALID_SOCKET };
    };

    static_assert(SocketDataConcept<DefaultSocketData<>>);
}

#include <Hermes/Socket/_base/Data/DefaultSocketData.tpp>