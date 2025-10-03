#pragma once

namespace Hermes {
    template<ByteLike Type>
    RawInputSocketRange<Type>::RawInputSocketRange(SOCKET socket)
    : _socket(socket) {
        _errorStatus = Receive();
    }

    template<ByteLike Type>
    RawInputSocketRange<Type>::Iterator RawInputSocketRange<Type>::begin() {
        return Iterator{this};
    }

    template<ByteLike Type>
    std::default_sentinel_t RawInputSocketRange<Type>::end() {
        return {};
    }

    template<ByteLike Type>
    RawInputSocketRange<Type>::Iterator::value_type RawInputSocketRange<Type>::Iterator::operator*() const {
        return view->_buffer[view->_index];
    }

    template<ByteLike Type>
    class RawInputSocketRange<Type>::Iterator& RawInputSocketRange<Type>::Iterator::operator++() {
        if (++view->_index >= view->_size)
            view->_errorStatus = view->Receive();
        return *this;
    }

    template<ByteLike Type>
    class RawInputSocketRange<Type>::Iterator& RawInputSocketRange<Type>::Iterator::operator++(int) {
        return ++(*this);
    }

    template<ByteLike Type>
    bool RawInputSocketRange<Type>::Iterator::operator==(std::default_sentinel_t) const {
        return static_cast<bool>(!view->_errorStatus);
    }

    template<ByteLike Type>
    ConnectionResultOper RawInputSocketRange<Type>::OptError() {
        return _errorStatus;
    }


    template<ByteLike Type>
    ConnectionResultOper RawInputSocketRange<Type>::Receive() {
        if (_socket == macroINVALID_SOCKET)
            return _errorStatus = std::unexpected{ ConnectionErrorEnum::SOCKET_NOT_OPEN };

        const int received = recv(_socket,
                                  reinterpret_cast<char*>(_buffer.data()), bufferSize, 0);

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
}