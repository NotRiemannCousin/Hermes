#pragma once
#include <Hermes/_base/Network.hpp>

namespace Hermes {
    template<SocketDataConcept Data>
    ConnectionResultOper DefaultConnectPolicy<Data>::Connect(Data& data, Options options) noexcept {
        auto addrRes{ data.endpoint.ToSockAddr() };
        if (!addrRes.has_value())
            return std::unexpected{ ConnectionErrorEnum::Unknown };

        auto [addr, addr_len, addrFamily]{ *addrRes };

        data.socket = socket(static_cast<int>(addrFamily), static_cast<int>(Data::Type), 0);

        if (data.socket < 0)
            return std::unexpected{ ConnectionErrorEnum::Unknown };

#pragma region Settings

        const auto s_applyOpt = [&](int level, int optName, auto value) {
            setsockopt(data.socket, level, optName, reinterpret_cast<const char*>(&value), sizeof(value));
        };

        if constexpr (Data::Family == AddressFamilyEnum::Inet6)
            s_applyOpt(IPPROTO_IPV6, IPV6_V6ONLY, int{ options.onlyIpv6 });

        if constexpr (Data::Type == SocketTypeEnum::Stream)
            if (options.tcpNoDelay) s_applyOpt(IPPROTO_TCP, TCP_NODELAY, 1);

        if (options.keepAlive)      s_applyOpt(SOL_SOCKET, SO_KEEPALIVE, 1);
        if (options.recvBufferSize) s_applyOpt(SOL_SOCKET, SO_RCVBUF, options.recvBufferSize);
        if (options.sendBufferSize) s_applyOpt(SOL_SOCKET, SO_SNDBUF, options.sendBufferSize);

        const auto s_connect_with_timeout = [&]() -> int {
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
                fd_set writeSet, errSet;
                FD_ZERO(&writeSet);
                FD_ZERO(&errSet);
                FD_SET(data.socket, &writeSet);
                FD_SET(data.socket, &errSet);

                timeval tv{
                    static_cast<long>(options.connectionTimeout.count() / 1000),
                    static_cast<long>((options.connectionTimeout.count() % 1000) * 1000)
                };

                const int selectRes{ select(static_cast<int>(data.socket) + 1, nullptr, &writeSet, &errSet, &tv) };

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
        };

#pragma endregion

        const int result{
            options.connectionTimeout.count() == 0
                ? connect(data.socket, reinterpret_cast<sockaddr*>(&addr), addr_len)
                : s_connect_with_timeout()
        };

        if (result == macroSOCKET_ERROR) {
            Close(data);
            return std::unexpected{ ConnectionErrorEnum::ConnectionFailed };
        }

        return {};
    }


    template<SocketDataConcept Data>
    void DefaultConnectPolicy<Data>::Close(Data& data) {
        shutdown(data.socket, static_cast<int>(SocketShutdownEnum::Send));
        closesocket(data.socket);
        data.socket = macroINVALID_SOCKET;
    }

    template<SocketDataConcept Data>
    void DefaultConnectPolicy<Data>::Abort(Data &data) {
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
