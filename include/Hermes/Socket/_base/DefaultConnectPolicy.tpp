#pragma once

namespace Hermes {
    template<SocketDataConcept Data>
    ConnectionResultOper DefaultConnectPolicy<Data>::Connect(Data& data) {
        auto addrRes{ data.endpoint.ToSockAddr() };
        if (!addrRes.has_value())
            return std::unexpected{ ConnectionErrorEnum::UNKNOWN };

        auto [addr, addr_len, addrFamily] = *addrRes;

        data.socket = socket(static_cast<int>(addrFamily), static_cast<int>(SocketTypeEnum::STREAM), 0);
        const int result{ connect(data.socket, reinterpret_cast<sockaddr*>(&addr), addr_len) };

        if (result == macroSOCKET_ERROR)
            return std::unexpected{ ConnectionErrorEnum::CONNECTION_FAILED };

        return {};
    }


    template<SocketDataConcept Data>
    ConnectionResultOper DefaultConnectPolicy<Data>::Close(Data& data) {
        if (data.socket != macroINVALID_SOCKET) {
            shutdown(data.socket, static_cast<int>(SocketShutdownEnum::BOTH));

            data.socket = macroINVALID_SOCKET;
        }

        return {};
    }
}