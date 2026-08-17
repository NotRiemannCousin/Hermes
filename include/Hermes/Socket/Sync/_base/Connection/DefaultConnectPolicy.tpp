#pragma once
#include <Hermes/_base/Network.hpp>
#include <algorithm>
#include <limits>

#ifndef _WIN32
#include <fcntl.h>
#include <netinet/tcp.h>
#include <poll.h>
#endif

namespace Hermes {
    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    template<SocketDataConcept Data>
    ConnectionResultOper DefaultConnectPolicy<Endpoint, SocketType, SocketFamily>::Connect(Data& data, Options options) noexcept {
        auto addrRes{ data.endpoint.ToSockAddr() };
        if (!addrRes.has_value())
            return std::unexpected{ ConnectionErrorEnum::Unknown };

        auto [addr, addr_len, addrFamily]{ *addrRes };

        data.socket = socket(static_cast<int>(addrFamily), static_cast<int>(SocketType), 0);

        if (data.socket < 0)
            return std::unexpected{ ConnectionErrorEnum::Unknown };

#pragma region Settings

        const auto applyOpt{ [&](const int level, const int optName, auto value) {
            setsockopt(data.socket, level, optName, reinterpret_cast<const char*>(&value), sizeof(value));
        } };

        if constexpr (SocketFamily == AddressFamilyEnum::Inet6)
            applyOpt(IPPROTO_IPV6, IPV6_V6ONLY, int{ options.onlyIpv6 });

        if constexpr (SocketType == SocketTypeEnum::Stream)
            if (options.tcpNoDelay) applyOpt(IPPROTO_TCP, TCP_NODELAY, 1);

        if (options.keepAlive)      applyOpt(SOL_SOCKET, SO_KEEPALIVE, 1);
        if (options.recvBufferSize) applyOpt(SOL_SOCKET, SO_RCVBUF, options.recvBufferSize);
        if (options.sendBufferSize) applyOpt(SOL_SOCKET, SO_SNDBUF, options.sendBufferSize);

        const auto connect_with_timeout{ [&]() -> int {
            #ifdef _WIN32
            u_long mode{ 1 };
            ioctlsocket(data.socket, FIONBIO, &mode);
            #else
            // Full Linux support... someday
            const int flags{ fcntl(data.socket, F_GETFL, 0) };
            fcntl(data.socket, F_SETFL, flags | O_NONBLOCK);
            #endif

            int res{ connect(data.socket, reinterpret_cast<sockaddr*>(&addr), addr_len) };

            bool inProgress{ false };
            if (res < 0) {
                #ifdef _WIN32
                inProgress = (WSAGetLastError() == WSAEWOULDBLOCK);
                #else
                inProgress = (errno == EINPROGRESS);
                #endif
            }

            if (inProgress) {
                const auto millis{ options.connectionTimeout };
                const int timeoutMs{ static_cast<int>(std::min<long long>(millis.count(), std::numeric_limits<int>::max())) };

#ifdef _WIN32
                WSAPOLLFD pfd{};
                pfd.fd = data.socket;
                pfd.events = POLLWRNORM;
                const int selectRes{ WSAPoll(&pfd, 1, timeoutMs) };
#else
                using PoolFd = pollfd;
                pollfd pfd{};
                pfd.fd = data.socket;
                pfd.events = POLLOUT;
                const int selectRes{ poll(&pfd, 1, timeoutMs) };
#endif

                if (selectRes > 0) {
                    int soError{ 0 };
                    socklen_t len{ sizeof(soError) };
                    getsockopt(data.socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soError), &len);

                    res = (soError == 0) ? 0 : -1;
                } else {
                    res = -1;
                }
            }

            #ifdef _WIN32
            mode = 0;
            ioctlsocket(data.socket, FIONBIO, &mode);
            #else
            fcntl(data.socket, F_SETFL, flags);
            #endif

            return res;
        } };

#pragma endregion

        const int result{
            options.connectionTimeout.count() == 0
                ? connect(data.socket, reinterpret_cast<sockaddr*>(&addr), addr_len)
                : connect_with_timeout()
        };

        if (result == macroSOCKET_ERROR) {
            Close(data);
            return std::unexpected{ ConnectionErrorEnum::ConnectionFailed };
        }

        return {};
    }


    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    template<SocketDataConcept Data>
    void DefaultConnectPolicy<Endpoint, SocketType, SocketFamily>::Close(Data& data) {

        shutdown(data.socket, static_cast<int>(SocketShutdownEnum::Send));
        CloseSocket(data.socket);
        data.socket = macroINVALID_SOCKET;
    }

    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    template<SocketDataConcept Data>
    void DefaultConnectPolicy<Endpoint, SocketType, SocketFamily>::Abort(Data& data) {
        constexpr linger lingerOption{ 1, 0 };

        setsockopt(
            data.socket,
            SOL_SOCKET,
            SO_LINGER,
            reinterpret_cast<const char*>(&lingerOption),
            sizeof(lingerOption)
        );

        CloseSocket(data.socket);
        data.socket = macroINVALID_SOCKET;
    }
}
