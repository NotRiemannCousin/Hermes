#pragma once
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>

#include <Hermes/Socket/_base/_base.hpp>

namespace Hermes {
    template<
            EndpointConcept Endpoint = IpEndpoint,
            SocketTypeEnum SocketType = SocketTypeEnum::STREAM,
            AddressFamilyEnum SocketFamily = AddressFamilyEnum::INET>
    struct DefaultSocketData {
        using EndpointType = Endpoint;

        static constexpr SocketTypeEnum Type = SocketType;
        static constexpr AddressFamilyEnum Family = SocketFamily;

        Endpoint endpoint{};
        SOCKET   socket{ macroINVALID_SOCKET };
    };

    static_assert(SocketDataConcept<DefaultSocketData<>>);
}
