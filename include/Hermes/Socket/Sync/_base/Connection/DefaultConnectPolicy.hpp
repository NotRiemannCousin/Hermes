#pragma once
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/Sync/_base/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {
    namespace _details {
        template <AddressFamilyEnum>
        struct ConnectOptionsIpv6Base {};

        template <>
        struct ConnectOptionsIpv6Base<AddressFamilyEnum::Inet6> {
            bool onlyIpv6{};
        };

        template <SocketTypeEnum>
        struct OptionsTcpNoDelayBase {};

        template <>
        struct OptionsTcpNoDelayBase<SocketTypeEnum::Stream> {
            bool tcpNoDelay{ true };
        };
    }


    template<SocketDataConcept Data = DefaultSocketData<>>
    struct DefaultConnectPolicy {
        struct Options : _details::ConnectOptionsIpv6Base<Data::Family>, _details::OptionsTcpNoDelayBase<Data::Type> {
            std::chrono::milliseconds connectionTimeout{};

            bool keepAlive{};

            int recvBufferSize{};
            int sendBufferSize{};
        };

        static ConnectionResultOper Connect(Data& data, Options options) noexcept;

        static void Close(Data& data);
        static void Abort(Data& data);
    };
}

#include <Hermes/Socket/Sync/_base/Connection/DefaultConnectPolicy.tpp>

namespace Hermes {
    static_assert(ConnectionPolicyConcept<DefaultConnectPolicy, DefaultSocketData<>>);
}