#pragma once
#include <Hermes/_base/Network.hpp>
#include <Family/StreamSocket.hpp>

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

    template<EndpointConcept EndpointType, template<class> class InputSocketView, class T>
        requires MinimalInputSocketRangeConcept<InputSocketView>
    StreamSocket<EndpointType, InputSocketView, T>::StreamSocket() noexcept = default;


    template<EndpointConcept EndpointType, template<class> class InputSocketView, class T>
        requires MinimalInputSocketRangeConcept<InputSocketView>
    StreamSocket<EndpointType, InputSocketView, T>::StreamSocket(StreamSocket &&other) noexcept :
        _socket{exchange(other._socket, macroINVALID_SOCKET)} {}

    template<EndpointConcept EndpointType, template<class> class InputSocketView, class T>
        requires MinimalInputSocketRangeConcept<InputSocketView>
    StreamSocket<EndpointType, InputSocketView, T> &StreamSocket<EndpointType, InputSocketView, T>::operator=(StreamSocket &&other) noexcept {
        if (this != &other)
            _socket = exchange(other._socket, macroINVALID_SOCKET);
        return *this;
    }

    template<EndpointConcept EndpointType, template<class> class InputSocketView, class T>
        requires MinimalInputSocketRangeConcept<InputSocketView>
    StreamSocket<EndpointType, InputSocketView, T>::~StreamSocket() { Close(); }

    template<EndpointConcept EndpointType, template<class> class InputSocketView, class T>
        requires MinimalInputSocketRangeConcept<InputSocketView>
    SOCKET & StreamSocket<EndpointType, InputSocketView, T>::UnsafeUnderlyingSocket() {
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



    template<EndpointConcept EndpointType, template<class> class InputSocketView, class T>
        requires MinimalInputSocketRangeConcept<InputSocketView>
    ConnectionResult<T> StreamSocket<EndpointType, InputSocketView, T>::Connect(const EndpointType &endpoint) {
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


    template<EndpointConcept EndpointType, template<class> class InputSocketView, class T>
        requires MinimalInputSocketRangeConcept<InputSocketView>
    StreamSent StreamSocket<EndpointType, InputSocketView, T>::SendRaw(ByteDataSpan data) const {
        if (_socket == macroINVALID_SOCKET)
            return unexpected{ ConnectionErrorEnum::SOCKET_NOT_OPEN };

        const int sent{ send(_socket, reinterpret_cast<const char*>(data.data()),
                        static_cast<int>(data.size()), 0) };

        if (sent == macroSOCKET_ERROR)
            return unexpected{ ConnectionErrorEnum::SEND_FAILED }; // TODO: This is disgusting... Improve error handling for send and receive

        return sent;
    }

    template<EndpointConcept EndpointType, template<class> class InputSocketView, class T>
        requires MinimalInputSocketRangeConcept<InputSocketView>
    StreamSent StreamSocket<EndpointType, InputSocketView, T>::SendStr(string_view data) const {
        return SendRaw(ByteDataSpan((std::byte*)(char*)data.data(), data.size()));
    }


    template<EndpointConcept EndpointType, template<class> class InputSocketView, class T>
        requires MinimalInputSocketRangeConcept<InputSocketView>
    InputSocketView<std::byte> StreamSocket<EndpointType, InputSocketView, T>::ReceiveRaw() const {
        return InputSocketView<std::byte>{ _socket };
    }

    template<EndpointConcept EndpointType, template<class> class InputSocketView, class T>
        requires MinimalInputSocketRangeConcept<InputSocketView>
    InputSocketView<char> StreamSocket<EndpointType, InputSocketView, T>::ReceiveStr() const {
        return InputSocketView<char>{ _socket };
    }

    template<EndpointConcept EndpointType, template<class> class InputSocketView, class T>
        requires MinimalInputSocketRangeConcept<InputSocketView>
    void StreamSocket<EndpointType, InputSocketView, T>::Close() const {
        if (_socket != macroINVALID_SOCKET)
            shutdown(_socket, static_cast<int>(SocketShutdownEnum::BOTH));
    }

    template<EndpointConcept EndpointType, template<class> class InputSocketView, class T>
        requires MinimalInputSocketRangeConcept<InputSocketView>
    bool StreamSocket<EndpointType, InputSocketView, T>::IsOpen() const {
        return _socket != macroINVALID_SOCKET;
    }
} // namespace Hermes
