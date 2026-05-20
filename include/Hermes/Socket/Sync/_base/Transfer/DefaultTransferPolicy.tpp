#pragma once
#include <print>

namespace Hermes {
    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    Byte DefaultTransferPolicy<SocketType>::RecvStream<Byte>::Iterator::operator*() const {
        if (view->_policy->_state->index >= view->_policy->_state->size)
            auto _{ view->Receive() };
        return std::bit_cast<Byte>(view->_policy->_state->buffer[view->_policy->_state->index]);
    }

    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    auto DefaultTransferPolicy<SocketType>::RecvStream<Byte>::Iterator::operator++() -> Iterator& {
        ++view->_policy->_state->index;
        return *this;
    }

    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    auto DefaultTransferPolicy<SocketType>::RecvStream<Byte>::Iterator::operator++(int) -> Iterator& {
        return ++*this;
    }

    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    bool DefaultTransferPolicy<SocketType>::RecvStream<Byte>::Iterator::operator==(std::default_sentinel_t) const {
        return (!view->_policy->_state->status && view->_policy->_state->index >= view->_policy->_state->size)
                || *view->_socket == macroINVALID_SOCKET;
    }


    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    template<SocketDataConcept Data>
    DefaultTransferPolicy<SocketType>::RecvStream<Byte>::RecvStream(Data& data, DefaultTransferPolicy& policy)
        : _socket{ &data.socket }, _policy{ &policy } {
        if (policy._state == nullptr)
            policy._state = std::make_unique<State>();
    }



    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    auto DefaultTransferPolicy<SocketType>::RecvStream<Byte>::begin() -> Iterator {
        return Iterator{ this };
    }

    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    std::default_sentinel_t DefaultTransferPolicy<SocketType>::RecvStream<Byte>::end() { return {}; }


    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    ConnectionResultOper DefaultTransferPolicy<SocketType>::RecvStream<Byte>::Error() const {
        return _policy->_state->status;
    }


    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    ConnectionResultOper DefaultTransferPolicy<SocketType>::RecvStream<Byte>::Receive() {
        StreamByteOper::second_type err{};
        auto& state{ _policy->_state };

        while (state->index >= state->size && err) {
            auto [newSize, errOp]{ DefaultTransferPolicy::RecvHelper<std::byte>(*_socket, state->buffer, RecvModeEnum::Any) };
            err = errOp;

            state->index -= state->size;
            state->size = newSize;
        }

        if (err.has_value())
            return {};
        state->status = err;
        state->buffer[state->size++] = {};

        if (err.error() == ConnectionErrorEnum::ConnectionClosed) {
            closesocket(*_socket);
            *_socket = macroINVALID_SOCKET;
        }

        return state->status;
    }


    template<SocketTypeEnum SocketType>
    template<SocketDataConcept Data, ByteLike Byte>
    StreamByteOper DefaultTransferPolicy<SocketType>::Recv(Data& data, std::span<Byte> bufferRecv, const RecvModeEnum recvMode) {
        if (_state != nullptr) {
            const auto size{ min(_state->size - _state->index, bufferRecv.size()) };
            std::memcpy(bufferRecv.data(), _state->buffer.data() + _state->index, size);
            _state->index += size;

            bufferRecv = bufferRecv.subspan(size);
            if (bufferRecv.empty())
                return { size, {} };
        }

        return DefaultTransferPolicy::RecvHelper(data.socket, bufferRecv, recvMode);
    }

    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    StreamByteOper DefaultTransferPolicy<SocketType>::RecvHelper(SOCKET& socket, std::span<Byte> bufferRecv, const RecvModeEnum recvMode) {
        if (socket == macroINVALID_SOCKET)
            return {0, std::unexpected{ ConnectionErrorEnum::SocketNotOpen } };
        size_t total{};
        do {
            const int received{ recv(socket,
                reinterpret_cast<char*>(bufferRecv.data() + total),
                static_cast<int>(bufferRecv.size() - total), 0) };
            if (received == 0) {
                closesocket(std::exchange(socket, macroINVALID_SOCKET));
                return { total, std::unexpected{ ConnectionErrorEnum::ConnectionClosed } };
            }

            if (received == macroSOCKET_ERROR)
                return { total, std::unexpected{ ConnectionErrorEnum::ReceiveFailed } };
            total += received;

            if (recvMode == RecvModeEnum::Any) break;
        } while (total < bufferRecv.size());
        return { total, {} };
    }

    template<SocketTypeEnum SocketType>
    template<SocketDataConcept Data, ByteLike Byte>
    StreamByteOper DefaultTransferPolicy<SocketType>::Send(Data& data, std::span<const Byte> bufferSend) {
        if (data.socket == macroINVALID_SOCKET)
            return { 0, std::unexpected{ ConnectionErrorEnum::SocketNotOpen } };
        size_t total{};
        while (total < bufferSend.size()) {
            const int sent{ send(data.socket,
                reinterpret_cast<const char*>(bufferSend.data() + total),
                static_cast<int>(bufferSend.size() - total), 0) };
            if (sent == macroSOCKET_ERROR)
                return { total, std::unexpected{ ConnectionErrorEnum::SendFailed } };
            total += sent;
        }
        return { total, {} };
    }

}