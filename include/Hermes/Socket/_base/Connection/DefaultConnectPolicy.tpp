#pragma once
#include <Hermes/_base/Network.hpp>

namespace Hermes {
    template<SocketDataConcept Data>
    ConnectionResultOper DefaultConnectPolicy<Data>::Connect(Data& data) {
        auto addrRes{ data.endpoint.ToSockAddr() };
        if (!addrRes.has_value())
            return std::unexpected{ ConnectionErrorEnum::Unknown };

        auto [addr, addr_len, addrFamily] = *addrRes;

        data.socket = socket(static_cast<int>(addrFamily), static_cast<int>(Data::Type), 0);
        const int result{ connect(data.socket, reinterpret_cast<sockaddr*>(&addr), addr_len) };

        if (result == macroSOCKET_ERROR)
            return std::unexpected{ ConnectionErrorEnum::ConnectionFailed };

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
