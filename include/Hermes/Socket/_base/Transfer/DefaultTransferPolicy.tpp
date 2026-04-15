#pragma once
#include <print>

namespace Hermes {
    template<SocketDataConcept Data>
    template<ByteLike Byte>
    DefaultTransferPolicy<Data>::template RecvLazyRange<Byte>::Iterator::value_type DefaultTransferPolicy<Data>::RecvLazyRange<Byte>::Iterator::operator*() const {
        if (view->_policy->_state->index >= view->_policy->_state->size)
            auto _{ view->Receive() };

        return std::bit_cast<Byte>(view->_policy->_state->buffer[view->_policy->_state->index]);
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    DefaultTransferPolicy<Data>::template RecvLazyRange<Byte>::Iterator& DefaultTransferPolicy<Data>::RecvLazyRange<Byte>::Iterator::operator++() {
        ++view->_policy->_state->index;
        return *this;
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    DefaultTransferPolicy<Data>::template RecvLazyRange<Byte>::Iterator& DefaultTransferPolicy<Data>::RecvLazyRange<Byte>::Iterator::operator++(int) {
        return ++*this;
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    bool DefaultTransferPolicy<Data>::RecvLazyRange<Byte>::Iterator::operator==(std::default_sentinel_t) const {
        return (!view->_policy->_state->status && view->_policy->_state->index >= view->_policy->_state->size)
                || view->_data->socket == macroINVALID_SOCKET;
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    DefaultTransferPolicy<Data>::RecvLazyRange<Byte>::RecvLazyRange(Data& data, DefaultTransferPolicy& policy)
        : _data { &data }, _policy{ &policy } {
        if (policy._state == nullptr)
            policy._state = std::make_unique<State>();
    }



    template<SocketDataConcept Data>
    template<ByteLike Byte>
    DefaultTransferPolicy<Data>::template RecvLazyRange<Byte>::Iterator DefaultTransferPolicy<Data>::RecvLazyRange<Byte>::begin() {
        return Iterator{ this };
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    std::default_sentinel_t DefaultTransferPolicy<Data>::RecvLazyRange<Byte>::end() { return {}; }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    ConnectionResultOper DefaultTransferPolicy<Data>::RecvLazyRange<Byte>::Error() const {
        return _policy->_state->status;
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    ConnectionResultOper DefaultTransferPolicy<Data>::RecvLazyRange<Byte>::Receive() {
        StreamByteOper::second_type err{};
        auto& state{ _policy->_state };

        while (state->index >= state->size && err) {
            auto [newSize, errOp]{ DefaultTransferPolicy::RecvHelper<std::byte>(*_data, state->buffer, true) };
            err = errOp;

            state->index -= state->size;
            state->size = newSize;
        }

        if (err.has_value())
            return {};

        state->status = err;
        state->buffer[state->size++] = {};

        if (err.error() == ConnectionErrorEnum::ConnectionClosed) {
            closesocket(_data->socket);
            _data->socket = macroINVALID_SOCKET;
        }

        return state->status;
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteOper DefaultTransferPolicy<Data>::Recv(Data& data, std::span<Byte> bufferRecv) {
        if (_state != nullptr) {
            const auto size{ min(_state->size - _state->index, bufferRecv.size()) };

            std::memcpy(bufferRecv.data(), _state->buffer.data() + _state->index, size);
            _state->index += size;

            bufferRecv = bufferRecv.subspan(size);
            if (bufferRecv.empty())
                return { size, {} };
        }

        return DefaultTransferPolicy::RecvHelper(data, bufferRecv, false);
    }
    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteOper DefaultTransferPolicy<Data>::RecvHelper(Data& data, std::span<Byte> bufferRecv, const bool single) {
        if (data.socket == macroINVALID_SOCKET)
            return {0, std::unexpected{ ConnectionErrorEnum::SocketNotOpen } };

        size_t total{};
        do {
            const int received{ recv(data.socket,
                reinterpret_cast<char*>(bufferRecv.data() + total),
                static_cast<int>(bufferRecv.size() - total), 0) };

            if (received == 0) {
                closesocket(std::exchange(data.socket, macroINVALID_SOCKET));
                return { total, std::unexpected{ ConnectionErrorEnum::ConnectionClosed } };
            }

            if (received == macroSOCKET_ERROR)
                return { total, std::unexpected{ ConnectionErrorEnum::ReceiveFailed } };

            total += received;
        } while (!single && total < bufferRecv.size());
        return { total, {} };
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteOper DefaultTransferPolicy<Data>::Send(Data& data, std::span<const Byte> bufferSend) {
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
