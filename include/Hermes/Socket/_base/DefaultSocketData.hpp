#pragma once
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>

#include <Hermes/Socket/_base/_base.hpp>

namespace Hermes {
    template<EndpointConcept Endpoint = IpEndpoint, AddressFamilyEnum SocketFamily = AddressFamilyEnum::INET>
    struct DefaultSocketData {
        using EndpointType = Endpoint;

        static constexpr AddressFamilyEnum Family = SocketFamily;

        Endpoint endpoint;
        SOCKET   socket;
    };

    static_assert(SocketDataConcept<DefaultSocketData<>>);
}
