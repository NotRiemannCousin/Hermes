#pragma once
#include <Hermes/Socket/_base/Transfer/TlsTransferStateMachine.hpp>

namespace Hermes {

    // ==============================================================================
    // RecvStream (View Interface)
    // ==============================================================================

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto TlsTransferPolicy<Data>::RecvStream<Byte>::Iterator::operator*() const -> value_type {
        if (view->_policy->_streamState->index >= view->_policy->_streamState->size)
            auto _{ view->Receive() };

        return std::bit_cast<Byte>(view->_policy->_streamState->buffer[view->_policy->_streamState->index]);
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto TlsTransferPolicy<Data>::RecvStream<Byte>::Iterator::operator++() -> Iterator& {
        ++view->_policy->_streamState->index;
        return *this;
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto TlsTransferPolicy<Data>::RecvStream<Byte>::Iterator::operator++(int) -> Iterator& {
        return ++*this;
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    bool TlsTransferPolicy<Data>::RecvStream<Byte>::Iterator::operator==(std::default_sentinel_t) const {
        const auto& state{ view->_policy->_streamState };

        return (!state->status && state->index >= state->size)
                || view->_data->socket == macroINVALID_SOCKET;
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::RecvStream<Byte>::RecvStream(Data& data, TlsTransferPolicy& policy)
        : _data{ &data }, _policy{ &policy } {
        if (policy._streamState == nullptr)
            policy._streamState = std::make_unique<StreamState>();
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto TlsTransferPolicy<Data>::RecvStream<Byte>::begin() -> Iterator {
        return Iterator{ this };
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    std::default_sentinel_t TlsTransferPolicy<Data>::RecvStream<Byte>::end() { return {}; }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    ConnectionResultOper TlsTransferPolicy<Data>::RecvStream<Byte>::Error() const {
        return _policy->_streamState->status;
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    ConnectionResultOper TlsTransferPolicy<Data>::RecvStream<Byte>::Receive() {
        StreamByteOper::second_type err{};
        auto& state{ _policy->_streamState };

        while (state->index >= state->size && err) {
            auto [newSize, errOp]{ _policy->Recv(*_data, std::span<std::byte>{state->buffer}, RecvModeEnum::Any) };
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


    // ==============================================================================
    // Transfer Policy Operations (Recv / Send)
    // ==============================================================================

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteOper TlsTransferPolicy<Data>::Recv(Data& data, std::span<Byte> bufferRecv, const RecvModeEnum recvMode) noexcept {
        size_t totalReceived{}, bytesReceived{};
        ConnectionResultOper err{};

        if (_streamState != nullptr) {
            const auto size{ (std::min)(static_cast<size_t>(_streamState->size - _streamState->index), bufferRecv.size()) };
            std::memcpy(bufferRecv.data(), _streamState->buffer.data() + _streamState->index, size);

            _streamState->index += size;
            totalReceived += size;
            bufferRecv     = bufferRecv.subspan(size);

            if (bufferRecv.empty())
                return { totalReceived, {} };
        }

        if (!data.transferStateMachine)
            data.transferStateMachine = std::make_unique<_details::TlsTransferStateMachine<Data, TlsTransferPolicy>>();


        data.transferStateMachine->StartToRecv(std::as_writable_bytes(bufferRecv), recvMode);
        do {
            RECV_INIT:
            data.transferStateMachine->SetToRecv();
            data.transferStateMachine->Advance(data);

            std::tie(bytesReceived, err) = data.transferStateMachine->GetResult();
            bufferRecv = bufferRecv.subspan(bytesReceived);
            totalReceived += bytesReceived;

            if (err)
                continue;

            if (err.error() != ConnectionErrorEnum::RenegotiationRequired)
                break;

            data.connectStateMachine->SetToOpen();

            if (!data.connectStateMachine->IsFinished())
                data.connectStateMachine->Advance(data);

            const auto hsResult{ data.connectStateMachine->GetResult() };

            if (!hsResult)
                return { totalReceived, std::unexpected{ hsResult.error() } };

            goto RECV_INIT;

        } while (recvMode == RecvModeEnum::All && !bufferRecv.empty());

        return { totalReceived, err };
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteOper TlsTransferPolicy<Data>::Send(Data& data, std::span<const Byte> bufferSend) noexcept {

        if (!data.transferStateMachine)
            data.transferStateMachine = std::make_unique<_details::TlsTransferStateMachine<Data, TlsTransferPolicy>>();

        data.transferStateMachine->StartToSend(std::as_bytes(bufferSend));

        if (!data.transferStateMachine->IsFinished())
            data.transferStateMachine->Advance(data);



        return data.transferStateMachine->GetResult();
    }

}