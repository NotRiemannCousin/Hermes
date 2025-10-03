#pragma once
#include <Hermes/_base/Network.hpp>

#include <experimental/generator>
#include <algorithm>
#include <expected>
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

    template<EndpointConcept EndpointType>
    DgramSocket<EndpointType>::DgramSocket() noexcept = default;

    template<EndpointConcept EndpointType>
    DgramSocket<EndpointType>::DgramSocket(DgramSocket &&other) noexcept :
        _socket{exchange(other._socket, macroINVALID_SOCKET)} {}

    template<EndpointConcept EndpointType>
    DgramSocket<EndpointType> &DgramSocket<EndpointType>::operator=(DgramSocket &&other) noexcept {
        if (this != &other)
            _socket = exchange(other._socket, macroINVALID_SOCKET);
        return *this;
    }

    template<EndpointConcept EndpointType>
    DgramSocket<EndpointType>::~DgramSocket() { Close(); }

    template<EndpointConcept EndpointType>
    SOCKET & DgramSocket<EndpointType>::UnsafeUnderlyingSocket() {
        return _socket;
    }


    //----------------------------------------------------------------------------------------------------
    // Connection
    //----------------------------------------------------------------------------------------------------

    template<EndpointConcept EndpointType>
    StreamSent DgramSocket<EndpointType>::Send(ByteDataSpan data, const EndpointType& endpoint) const {
        if (_socket == macroINVALID_SOCKET)
            return unexpected{ ConnectionErrorEnum::SOCKET_NOT_OPEN };

        auto [addr, _] = endpoint.ToSockAddr();

        const int sent{ sendto(_socket, reinterpret_cast<const char*>(data.data()),
                        static_cast<int>(data.size()), 0, addr) };

        if (sent == macroSOCKET_ERROR)
            return unexpected{ ConnectionErrorEnum::SEND_FAILED }; // TODO: This is disgusting... Improve error handling for send and receive

        return sent;
    }


    template<EndpointConcept EndpointType>
    ConnectionResult<ByteData> DgramSocket<EndpointType>::ReceiveAll() const {
        ByteData buffer{};
        buffer.reserve(0xF000);

        for (auto &&data: Receive()) {
            if (!data)
                return std::unexpected{data.error()};
            buffer.append_range(*data);
        }

        return buffer;
    }

    template<EndpointConcept EndpointType>
    std::pair<DataStream, EndpointType> DgramSocket<EndpointType>::Receive() const {
        if (_socket == macroINVALID_SOCKET) {
            co_yield std::unexpected{ ConnectionErrorEnum::SOCKET_NOT_OPEN };
            co_return;
        }

        ByteData packageBuffer(0xF000);
        sockaddr addr;

        while (true) {
            const int received{ recvfrom(_socket, reinterpret_cast<char*>(packageBuffer.data()),
                                      static_cast<int>(packageBuffer.size()), 0,
                                      &addr, nullptr) };

            if (received == macroSOCKET_ERROR) {
                const int error{ WSAGetLastError() };
                if (error == WSAEWOULDBLOCK)
                    co_yield std::unexpected{ConnectionErrorEnum::CONNECTION_TIMEOUT};
                else
                    co_yield std::unexpected{ConnectionErrorEnum::RECEIVE_FAILED};
                co_return;
            }

            co_yield { packageBuffer, addr };
        }
    }

    template<EndpointConcept EndpointType>
    void DgramSocket<EndpointType>::Close() const {
        if (_socket != macroINVALID_SOCKET)
            shutdown(_socket, static_cast<int>(SocketShutdownEnum::BOTH));
    }

    template<EndpointConcept EndpointType>
    bool DgramSocket<EndpointType>::IsOpen() const {
        return _socket != macroINVALID_SOCKET;
    }
} // namespace Hermes
