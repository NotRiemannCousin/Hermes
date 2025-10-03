#pragma once

namespace Hermes {
    template<class Type>
    RawInputSocketRange<Type>::RawInputSocketRange(SOCKET socket)
    : _socket(socket) {
        errorStatus = Receive();
    }

    template<class Type>
    RawInputSocketRange<Type>::Iterator RawInputSocketRange<Type>::begin() {
        return Iterator{this};
    }

    template<class Type>
    std::default_sentinel_t RawInputSocketRange<Type>::end() {
        return {};
    }

    template<class Type>
    Type RawInputSocketRange<Type>::Iterator::operator*() const {
        return view->_buffer[view->index];
    }

    template<class Type>
    class RawInputSocketRange<Type>::Iterator& RawInputSocketRange<Type>::Iterator::operator++() {
        if (++view->index >= view->size)
            view->errorStatus = view->Receive();
        return *this;
    }

    template<class Type>
    class RawInputSocketRange<Type>::Iterator& RawInputSocketRange<Type>::Iterator::operator++(int) {
        ++(*this);
    }

    template<class Type>
    bool RawInputSocketRange<Type>::Iterator::operator==(std::default_sentinel_t) const {
        return static_cast<bool>(!view->errorStatus);
    }

    template<class Type>
    ConnectionResultOper RawInputSocketRange<Type>::OptError() {
        return errorStatus;
    }


    template<class Type>
    ConnectionResultOper RawInputSocketRange<Type>::Receive() {
        if (_socket == macroINVALID_SOCKET)
            return errorStatus = std::unexpected{ ConnectionErrorEnum::SOCKET_NOT_OPEN };

        const int received = recv(_socket,
                                  reinterpret_cast<char*>(_buffer.data()),
                                  static_cast<int>(_buffer.size()), 0);

        size  = received;
        index = 0;

        if (received != macroSOCKET_ERROR)
            return {};

        return errorStatus = std::unexpected{
            WSAGetLastError() == WSAEWOULDBLOCK
            ? ConnectionErrorEnum::CONNECTION_TIMEOUT
            : ConnectionErrorEnum::RECEIVE_FAILED
        };
    }
}