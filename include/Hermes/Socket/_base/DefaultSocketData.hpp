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
        ~DefaultSocketData() = default;

        DefaultSocketData(DefaultSocketData&& other) noexcept : endpoint(std::move(other.endpoint)),
                socket(std::exchange(other.socket, macroINVALID_SOCKET)) {}

        DefaultSocketData& operator=(DefaultSocketData&& other) noexcept {
            // I will not create a tpp just for this

            if (this != &other) {
                endpoint = std::move(other.endpoint);
                socket   = std::exchange(other.socket, macroINVALID_SOCKET);
            }
            return *this;
        }

        Endpoint endpoint{};
        SOCKET   socket{ macroINVALID_SOCKET };
    };

    static_assert(SocketDataConcept<DefaultSocketData<>>);
}
