#pragma once
#include <Hermes/_base/Network.hpp>

#include <experimental/generator>
#include <algorithm>
#include <expected>
#include <ranges>
#include <sstream>

using std::array;
using std::string;
using std::string_view;
using std::vector;
using std::experimental::generator;

using std::unexpected;
using std::expected;
using std::optional;


namespace Hermes {
    using std::exchange;
    using std::move;
    //----------------------------------------------------------------------------------------------------
    // Construct
    //----------------------------------------------------------------------------------------------------

    template<EndpointConcept EndpointType, typename T>
    StreamSocket<EndpointType, T>::StreamSocket() noexcept = default;


    template<EndpointConcept EndpointType, typename T>
    StreamSocket<EndpointType, T>::StreamSocket(StreamSocket &&other) noexcept :
        _socket{exchange(other._socket, macroINVALID_SOCKET)} {}

    template<EndpointConcept EndpointType, typename T>
    StreamSocket<EndpointType, T> &StreamSocket<EndpointType, T>::operator=(StreamSocket &&other) noexcept {
        if (this != &other)
            _socket = exchange(other._socket, macroINVALID_SOCKET);
        return *this;
    }

    template<EndpointConcept EndpointType, typename T>
    StreamSocket<EndpointType, T>::~StreamSocket() { Close(); }

    template<EndpointConcept EndpointType, typename T>
    SOCKET & StreamSocket<EndpointType, T>::UnsafeUnderlyingSocket() {
        return _socket;
    }


    //----------------------------------------------------------------------------------------------------
    // Connection
    //----------------------------------------------------------------------------------------------------

    static std::byte StrToBytes(char c) {
        return static_cast<std::byte>(c);
    }
    static char BytesToStr(std::byte c) {
        return static_cast<char>(c);
    }



    template<EndpointConcept EndpointType, typename T>
    ConnectionResult<T> StreamSocket<EndpointType, T>::Connect(const EndpointType &endpoint) {
        auto addrRes{ endpoint.ToSockAddr() };
        if (!addrRes.has_value())
            return unexpected{ ConnectionErrorEnum::UNKNOWN };

        auto [addr, addr_len, addrFamily] = *addrRes;

        SOCKET _socket = socket(static_cast<int>(addrFamily), static_cast<int>(SocketTypeEnum::STREAM), 0);
        const int result = connect(_socket, reinterpret_cast<sockaddr*>(&addr), addr_len);

        if (result == macroSOCKET_ERROR)
            return unexpected{ ConnectionErrorEnum::CONNECTION_FAILED };

        T t{};
        t._socket = _socket;
        return t;
    }


    template<EndpointConcept EndpointType, typename T>
    StreamSent StreamSocket<EndpointType, T>::SendRaw(ByteDataSpan data) const {
        if (_socket == macroINVALID_SOCKET)
            return unexpected{ ConnectionErrorEnum::SOCKET_NOT_OPEN };

        const int sent{ send(_socket, reinterpret_cast<const char*>(data.data()),
                        static_cast<int>(data.size()), 0) };

        if (sent == macroSOCKET_ERROR)
            return unexpected{ ConnectionErrorEnum::SEND_FAILED }; // TODO: This is disgusting... Improve error handling for send and receive

        return sent;
    }

    template<EndpointConcept EndpointType, typename T>
    StreamSent StreamSocket<EndpointType, T>::SendStr(string_view data) const {
        return SendRaw(ByteDataSpan((std::byte*)(char*)data.data(), data.size()));
    }


    template<EndpointConcept EndpointType, typename T>
    DataStream StreamSocket<EndpointType, T>::ReceiveRaw() const {
        if (_socket == macroINVALID_SOCKET) {
            co_yield std::unexpected{ ConnectionErrorEnum::SOCKET_NOT_OPEN };
            co_return;
        }

        ByteData bufferRecv(0xF000);

        while (true) {
            const int received{ recv(_socket, reinterpret_cast<char*>(bufferRecv.data()),
                                      static_cast<int>(bufferRecv.size()), 0) };

            if (received == macroSOCKET_ERROR) {
                const int error{ WSAGetLastError() };
                if (error == WSAEWOULDBLOCK)
                    co_yield std::unexpected{ConnectionErrorEnum::CONNECTION_TIMEOUT};
                else
                    co_yield std::unexpected{ConnectionErrorEnum::RECEIVE_FAILED};
                co_return;
            }

            co_yield bufferRecv;
        }
    }

    template<EndpointConcept EndpointType, typename T>
    DataStringStream StreamSocket<EndpointType, T>::ReceiveStr() const {

        static auto toString = [](const ByteData &rec) {
            return rec | std::views::transform(BytesToStr)
                    | std::ranges::to<string>();
        };

        for (ConnectionResult received : ReceiveRaw())
            co_yield received.transform(toString);
    }

    template<EndpointConcept EndpointType, typename T>
    void StreamSocket<EndpointType, T>::Close() const {
        if (_socket != macroINVALID_SOCKET)
            shutdown(_socket, static_cast<int>(SocketShutdownEnum::BOTH));
    }

    template<EndpointConcept EndpointType, typename T>
    bool StreamSocket<EndpointType, T>::IsOpen() const {
        return _socket != macroINVALID_SOCKET;
    }

    template<EndpointConcept EndpointType, typename T>
    ConnectionResult<ByteData> StreamSocket<EndpointType, T>::ReceiveAllRaw() const {
        ByteData buffer{};
        buffer.reserve(0xF000);

        for (auto &&data: ReceiveRaw()) {
            if (!data)
                return std::unexpected{data.error()};
            buffer.append_range(*data);
        }

        return buffer;
    }

    template<EndpointConcept EndpointType, typename T>
    ConnectionResult<std::string> StreamSocket<EndpointType, T>::ReceiveAllStr() const {
        static auto toString = [](const ByteData &rec) {
            return rec | std::views::transform(BytesToStr)
                    | std::ranges::to<string>();
        };

        return ReceiveAllRaw().transform(toString);
    }
} // namespace Hermes
