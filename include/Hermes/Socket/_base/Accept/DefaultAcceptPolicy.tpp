#pragma once
#include <Hermes/_base/Network.hpp>

namespace Hermes {

    template<SocketDataConcept Data>
    ConnectionResultOper DefaultAcceptPolicy<Data>::Listen(Data& data, int backlog, ListenOptions options) noexcept {
        auto addrRes{ data.endpoint.ToSockAddr() };
        if (!addrRes)
            return std::unexpected{ ConnectionErrorEnum::InvalidEndpoint };

        auto [addr, addrLen, addrFamily]{ *addrRes };

        data.socket = socket(static_cast<int>(addrFamily), static_cast<int>(Data::Type), 0);
        if (data.socket == macroINVALID_SOCKET)
            return std::unexpected{ ConnectionErrorEnum::ConnectionFailed };

#pragma region Listen Options

        const auto s_applyOpt = [&](int level, int optName, auto value) {
            setsockopt(data.socket, level, optName, reinterpret_cast<const char*>(&value), sizeof(value));
        };

        if constexpr (Data::Family == AddressFamilyEnum::Inet6)
            s_applyOpt(IPPROTO_IPV6, IPV6_V6ONLY, int{ options.onlyIpv6 });

        if (options.reuseAddress)   s_applyOpt(SOL_SOCKET, SO_REUSEADDR, 1);
        if (options.recvBufferSize) s_applyOpt(SOL_SOCKET, SO_RCVBUF, options.recvBufferSize);
        if (options.sendBufferSize) s_applyOpt(SOL_SOCKET, SO_SNDBUF, options.sendBufferSize);

#pragma endregion

        if (bind(data.socket, reinterpret_cast<sockaddr*>(&addr), static_cast<int>(addrLen)) == macroSOCKET_ERROR) {
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
    ConnectionResultOper DefaultAcceptPolicy<Data>::Accept(Data& listenData, Data& outData, AcceptOptions options) noexcept {
        sockaddr_storage clientAddr{};
        int clientAddrLen{ sizeof(clientAddr) };

        outData.socket = accept(listenData.socket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);

        if (outData.socket == macroINVALID_SOCKET)
            return std::unexpected{ ConnectionErrorEnum::ConnectionFailed };

#pragma region Accept Options

        auto s_applyOpt = [&](int level, int optName, auto value) {
            ::setsockopt(outData.socket, level, optName, reinterpret_cast<const char*>(&value), sizeof(value));
        };

        if constexpr (Data::Type == SocketTypeEnum::Stream)
            if (options.tcpNoDelay) s_applyOpt(IPPROTO_TCP, TCP_NODELAY, 1);

        if (options.keepAlive)      s_applyOpt(SOL_SOCKET, SO_KEEPALIVE, 1);
        if (options.recvBufferSize) s_applyOpt(SOL_SOCKET, SO_RCVBUF, options.recvBufferSize);
        if (options.sendBufferSize) s_applyOpt(SOL_SOCKET, SO_SNDBUF, options.sendBufferSize);

#pragma endregion

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
