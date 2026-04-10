#pragma once
#include <Hermes/_base/Network.hpp>

namespace Hermes {

    template<SocketDataConcept Data>
    ConnectionResultOper DefaultAcceptPolicy<Data>::Listen(Data& data, int backlog) noexcept {
        Network::Initialize();

        auto addrRes{ data.endpoint.ToSockAddr() };
        if (!addrRes)
            return std::unexpected{ ConnectionErrorEnum::InvalidEndpoint };

        auto [addr, addr_len, addrFamily]{ *addrRes };

        data.socket = socket(static_cast<int>(addrFamily), static_cast<int>(Data::Type), 0);
        if (data.socket == macroINVALID_SOCKET)
            return std::unexpected{ ConnectionErrorEnum::ConnectionFailed };

        if constexpr (Data::Family == AddressFamilyEnum::Inet6) {
            int opt{}; // TODO: FUTURE: Implement a proper way to set options inside of data
            setsockopt(data.socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char*>(&opt), sizeof(opt));
        }

        if (bind(data.socket, reinterpret_cast<sockaddr*>(&addr), static_cast<int>(addr_len)) == macroSOCKET_ERROR) {
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return std::unexpected{ ConnectionErrorEnum::AddressInUse };
        }

        if (listen(data.socket, backlog) == macroSOCKET_ERROR) {
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return std::unexpected{ ConnectionErrorEnum::ListenFailed };
        }

        return {};
    }


    template<SocketDataConcept Data>
    ConnectionResultOper DefaultAcceptPolicy<Data>::Accept(Data& listenData, Data& outData) noexcept {
        sockaddr_storage clientAddr{};
        int clientAddrLen{ sizeof(clientAddr) };

        outData.socket = accept(listenData.socket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);

        if (outData.socket == macroINVALID_SOCKET)
            return std::unexpected{ ConnectionErrorEnum::ConnectionFailed };

        auto endpointRes{ Data::EndpointType::FromSockAddr(
            SocketInfoAddr{ clientAddr, static_cast<size_t>(clientAddrLen), AddressFamilyEnum{ clientAddr.ss_family } }) };

        if (!endpointRes) {
            closesocket(outData.socket);
            outData.socket = macroINVALID_SOCKET;
            return std::unexpected{ ConnectionErrorEnum::InvalidEndpoint };
        }

        outData.endpoint = std::move(*endpointRes);
        return {};
    }


    template<SocketDataConcept Data>
    void DefaultAcceptPolicy<Data>::Close(Data& data) noexcept {
        shutdown(data.socket, static_cast<int>(SocketShutdownEnum::Send));
        closesocket(data.socket);
        data.socket = macroINVALID_SOCKET;
    }

    template<SocketDataConcept Data>
    void DefaultAcceptPolicy<Data>::Abort(Data &data) noexcept {
        constexpr linger lingerOption{ 1, 0 };

        setsockopt(
            data.socket,
            SOL_SOCKET,
            SO_LINGER,
            reinterpret_cast<const char*>(&lingerOption),
            sizeof(lingerOption)
        );

        closesocket(data.socket);
        data.socket = macroINVALID_SOCKET;
    }
}
