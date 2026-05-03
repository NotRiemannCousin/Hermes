#pragma once
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>

#include <chrono>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {
    template<
            EndpointConcept Endpoint = IpEndpoint,
            SocketTypeEnum SocketType = SocketTypeEnum::Stream,
            AddressFamilyEnum SocketFamily = AddressFamilyEnum::Inet6>
    struct DefaultSocketData {
        using EndpointType = Endpoint;

        static constexpr SocketTypeEnum Type = SocketType;
        static constexpr AddressFamilyEnum Family = SocketFamily;

        DefaultSocketData() = default;
        explicit DefaultSocketData(Endpoint other);
        ~DefaultSocketData() = default;

        DefaultSocketData(DefaultSocketData&& other) noexcept;
        DefaultSocketData& operator=(DefaultSocketData&& other) noexcept;

        DefaultSocketData MakeChild() const;


        struct SocketOptions {
            // I can't do `using std::chrono_literals::...` thing here, aff
            std::chrono::milliseconds connectTimeout{ std::chrono::seconds{ 30 } };
            std::chrono::milliseconds sendTimeout{ std::chrono::seconds{ 10 } };
            std::chrono::milliseconds recvTimeout{ std::chrono::seconds{ 10 } };
        };

        struct State {
            std::array<std::byte, 0x4000> buffer{};
        };


        Endpoint endpoint{};
        SOCKET   socket{ macroINVALID_SOCKET };

        std::unique_ptr<State> state{};
        SocketOptions options{};
    };

    static_assert(SocketDataConcept<DefaultSocketData<>>);
}

#include <Hermes/Socket/Sync/_base/Data/DefaultSocketData.tpp>