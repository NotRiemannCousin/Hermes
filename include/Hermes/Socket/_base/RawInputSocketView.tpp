#pragma once

namespace Hermes {
    template<class Type>
    RawInputSocketView<Type>::RawInputSocketView(SOCKET socket)
    : _socket(socket) {
        errorStatus = Receive();
    }

    template<class Type>
    RawInputSocketView<Type>::Iterator RawInputSocketView<Type>::begin() {
        return Iterator{this};
    }

    template<class Type>
    std::default_sentinel_t RawInputSocketView<Type>::end() {
        return {};
    }

    template<class Type>
    Type RawInputSocketView<Type>::Iterator::operator*() const {
        return view->_buffer[view->index];
    }

    template<class Type>
    class RawInputSocketView<Type>::Iterator& RawInputSocketView<Type>::Iterator::operator++() {
        if (++view->index >= view->size)
            view->errorStatus = view->Receive();
        return *this;
    }


    template<class Type>
    void RawInputSocketView<Type>::Iterator::operator++(int) {
        ++(*this);
    }

    template<class Type>
    bool RawInputSocketView<Type>::Iterator::operator==(std::default_sentinel_t) const {
        return static_cast<bool>(!view->errorStatus);
    }

    template<class Type>
    ConnectionResultOper RawInputSocketView<Type>::OptError() {
        return errorStatus;
    }


    template<class Type>
    ConnectionResultOper RawInputSocketView<Type>::Receive() {
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