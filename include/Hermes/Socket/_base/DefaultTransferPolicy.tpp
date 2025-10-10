#pragma once
#include <print>

#include "DefaultTransferPolicy.hpp"

namespace Hermes {
    template<SocketDataConcept Data>
    template<ByteLike Byte>
    DefaultTransferPolicy<Data>::template RecvRange<Byte>::Iterator::value_type DefaultTransferPolicy<Data>::RecvRange<Byte>::Iterator::operator*() const {
        if (view->_index == view->_size)
            view->Receive();

        return std::bit_cast<Byte>(view->_buffer[view->_index]);
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    DefaultTransferPolicy<Data>::template RecvRange<Byte>::Iterator& DefaultTransferPolicy<Data>::RecvRange<Byte>::Iterator::operator++() {
        if (++view->_index >= view->_size)
            view->Receive();

        return *this;
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    DefaultTransferPolicy<Data>::template RecvRange<Byte>::Iterator& DefaultTransferPolicy<Data>::RecvRange<Byte>::Iterator::operator++(int) {
        return ++(*this);
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    bool DefaultTransferPolicy<Data>::RecvRange<Byte>::Iterator::operator==(std::default_sentinel_t) const {
        return !view->_errorStatus.has_value() || (view->_data.socket == macroINVALID_SOCKET);
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    DefaultTransferPolicy<Data>::RecvRange<Byte>::RecvRange(Data& data, DefaultTransferPolicy&) : _data { data } { }



    template<SocketDataConcept Data>
    template<ByteLike Byte>
    DefaultTransferPolicy<Data>::template RecvRange<Byte>::Iterator DefaultTransferPolicy<Data>::RecvRange<Byte>::begin() {
        return Iterator{this};
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    std::default_sentinel_t DefaultTransferPolicy<Data>::RecvRange<Byte>::end() { return {}; }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    ConnectionResultOper DefaultTransferPolicy<Data>::RecvRange<Byte>::Error() const {
        return _errorStatus;
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    ConnectionResultOper DefaultTransferPolicy<Data>::RecvRange<Byte>::Receive() {
        if (_data.socket == macroINVALID_SOCKET)
            return _errorStatus = std::unexpected{ ConnectionErrorEnum::SOCKET_NOT_OPEN };

        const int received{ recv(_data.socket,
                                  reinterpret_cast<char*>(_buffer.data()), bufferSize, 0) };

        _size = received;
        _index = 0;

        if (received != macroSOCKET_ERROR)
            return {};

        return _errorStatus = std::unexpected{
            WSAGetLastError() == WSAEWOULDBLOCK
            ? ConnectionErrorEnum::CONNECTION_TIMEOUT
            : ConnectionErrorEnum::RECEIVE_FAILED
        };
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteCount DefaultTransferPolicy<Data>::Recv(Data& data, std::span<Byte> bufferRecv) {
        if (data.socket == macroINVALID_SOCKET)
            return std::unexpected{ConnectionErrorEnum::SOCKET_NOT_OPEN};

        size_t total{};
        while (total < bufferRecv.size()) {
            int received = recv(data.socket,
                reinterpret_cast<char*>(bufferRecv.data() + total),
                static_cast<int>(bufferRecv.size() - total), 0);

            if (received == 0)
                break;
            if (received == macroSOCKET_ERROR)
                return std::unexpected{ConnectionErrorEnum::RECEIVE_FAILED};

            total += received;
        }
        return { total };
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteCount DefaultTransferPolicy<Data>::Send(Data& data, std::span<const Byte> bufferSend) {
        if (data.socket == macroINVALID_SOCKET)
            return std::unexpected{ConnectionErrorEnum::SOCKET_NOT_OPEN};

        size_t total{};
        while (total < bufferSend.size()) {
            int sent = send(data.socket,
                reinterpret_cast<const char*>(bufferSend.data() + total),
                static_cast<int>(bufferSend.size() - total), 0);

            if (sent == macroSOCKET_ERROR)
                return std::unexpected{ConnectionErrorEnum::SEND_FAILED};

            total += sent;
        }
        return { total };
    }

}
