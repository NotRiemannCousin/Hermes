#pragma once


namespace Hermes {
    template<SocketDataConcept Data>
    typename DefaultTransferPolicy<Data>::RecvRange::Iterator::value_type DefaultTransferPolicy<Data>::RecvRange::Iterator::operator*() const {
        return view->_buffer[view->_index];
    }

    template<SocketDataConcept Data>
    typename DefaultTransferPolicy<Data>::RecvRange::Iterator& DefaultTransferPolicy<Data>::RecvRange::Iterator::operator++() {
        if (++view->_index >= view->_buffer.size())
            view->_index = 0;
        return *this;
    }

    template<SocketDataConcept Data>
    typename DefaultTransferPolicy<Data>::RecvRange::Iterator& DefaultTransferPolicy<Data>::RecvRange::Iterator::operator++(int) {
        return ++(*this);
    }

    template<SocketDataConcept Data>
    bool DefaultTransferPolicy<Data>::RecvRange::Iterator::operator==(std::default_sentinel_t) const {
        return static_cast<bool>(!view->_errorStatus);
    }


    template<SocketDataConcept Data>
    DefaultTransferPolicy<Data>::RecvRange::RecvRange(DefaultTransferPolicy& policy) : _policy{ policy } { }



    template<SocketDataConcept Data>
    typename DefaultTransferPolicy<Data>::RecvRange::Iterator DefaultTransferPolicy<Data>::RecvRange::begin() {
        return Iterator{this};
    }

    template<SocketDataConcept Data>
    std::default_sentinel_t DefaultTransferPolicy<Data>::RecvRange::end() {
        return {};
    }

    // template<SocketDataConcept Data>
    // ConnectionResultOper DefaultTransferPolicy<Data>::RecvRange::Flush() {
    //     if (socket == macroINVALID_SOCKET)
    //         return unexpected{ ConnectionErrorEnum::SOCKET_NOT_OPEN };
    //
    //     const int sent{ send(socket, reinterpret_cast<const char*>(data.data()),
    //                     static_cast<int>(data.size()), 0) };
    //
    //     if (sent == macroSOCKET_ERROR)
    //         return unexpected{ ConnectionErrorEnum::SEND_FAILED }; TODO: This is disgusting... Improve error handling for send and receive
    //
    //     index = 0;
    // }
    //
    template<SocketDataConcept Data>
    ConnectionResultOper DefaultTransferPolicy<Data>::RecvRange::OptError() const {
        return _errorStatus;
    }


    template<SocketDataConcept Data>
    ConnectionResultOper DefaultTransferPolicy<Data>::RecvRange::Receive() {
        if (_policy.socket == macroINVALID_SOCKET)
            return _errorStatus = std::unexpected{ ConnectionErrorEnum::SOCKET_NOT_OPEN };

        const int received = recv(_policy.socket,
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
