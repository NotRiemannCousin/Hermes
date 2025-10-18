#pragma once
#include <print>

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
        ++view->_index;
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
        return !view->_errorStatus && view->_index == view->_size;
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

        auto [newSize, err]{ DefaultTransferPolicy::RecvHelper<std::byte>(_data, _buffer, true) };

        _size = newSize;
        _index = 0;

        if (err) return {};

        _errorStatus = std::unexpected{ err.error() };

        if (err.error() == ConnectionErrorEnum::CONNECTION_CLOSED)
            return {};

        // if (received != macroSOCKET_ERROR) return {};

        return std::unexpected{ ConnectionErrorEnum::RECEIVE_FAILED };;
        // return _errorStatus = std::unexpected{
        //     WSAGetLastError() == WSAEWOULDBLOCK
        //     ? ConnectionErrorEnum::CONNECTION_TIMEOUT
        //     : ConnectionErrorEnum::RECEIVE_FAILED
        // };
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteOper DefaultTransferPolicy<Data>::Recv(Data& data, std::span<Byte> bufferRecv) {
        return DefaultTransferPolicy::RecvHelper(data, bufferRecv, false);
    }
    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteOper DefaultTransferPolicy<Data>::RecvHelper(Data& data, std::span<Byte> bufferRecv, bool single) {
        if (data.socket == macroINVALID_SOCKET)
            return {0, std::unexpected{ ConnectionErrorEnum::SOCKET_NOT_OPEN } };

        size_t total{};
        do {
            const int received{ recv(data.socket,
                reinterpret_cast<char*>(bufferRecv.data() + total),
                static_cast<int>(bufferRecv.size() - total), 0) };

            if (received == 0) {
                closesocket(std::exchange(data.socket, macroINVALID_SOCKET));
                return { total, std::unexpected{ ConnectionErrorEnum::CONNECTION_CLOSED } };
            }

            if (received == macroSOCKET_ERROR)
                return { total, std::unexpected{ ConnectionErrorEnum::RECEIVE_FAILED } };

            total += received;
        } while (!single && total < bufferRecv.size());
        return { total, {} };
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteOper DefaultTransferPolicy<Data>::Send(Data& data, std::span<const Byte> bufferSend) {
        if (data.socket == macroINVALID_SOCKET)
            return { 0, std::unexpected{ ConnectionErrorEnum::SOCKET_NOT_OPEN } };

        size_t total{};
        while (total < bufferSend.size()) {
            const int sent{ send(data.socket,
                reinterpret_cast<const char*>(bufferSend.data() + total),
                static_cast<int>(bufferSend.size() - total), 0) };

            if (sent == macroSOCKET_ERROR)
                return { total, std::unexpected{ ConnectionErrorEnum::SEND_FAILED } };

            total += sent;
        }
        return { total, {} };
    }

}
