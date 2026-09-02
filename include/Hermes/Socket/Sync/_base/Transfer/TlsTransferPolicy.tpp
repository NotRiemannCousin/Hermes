#pragma once
#include <Hermes/Socket/_base/Transfer/TlsTransferStateMachine.hpp>

namespace Hermes {

    // ==============================================================================
    // RecvStream (View Interface)
    // ==============================================================================

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto TlsTransferPolicy<Data>::RecvStream<Byte>::Iterator::operator*() const -> value_type {
        if (view->m_policy->m_streamState->index >= view->m_policy->m_streamState->size)
            auto _{ view->Receive() };

        return static_cast<Byte>(view->m_policy->m_streamState->buffer[view->m_policy->m_streamState->index]);
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto TlsTransferPolicy<Data>::RecvStream<Byte>::Iterator::operator++() -> Iterator& {
        ++view->m_policy->m_streamState->index;
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
        const auto& state{ view->m_policy->m_streamState };

        return (!state->status && state->index >= state->size)
                || view->m_data->socket == macroINVALID_SOCKET;
    }

        template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::RecvStream<Byte>::RecvStream(Data& data, TlsTransferPolicy& policy)
        requires std::default_initializable<RecvOptions>
        : RecvStream{ data, policy, RecvOptions{} } { }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::RecvStream<Byte>::RecvStream(Data& data, TlsTransferPolicy& policy, RecvOptions options)
        : m_data{ &data }, m_policy{ &policy }, m_options{ options } {
        if (policy.m_streamState == nullptr)
            policy.m_streamState = std::make_unique<StreamState>();
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
        return m_policy->m_streamState->status;
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    ConnectionResultOper TlsTransferPolicy<Data>::RecvStream<Byte>::Receive() {
        StreamByteOper::second_type err{};
        auto& state{ m_policy->m_streamState };

        while (state->index >= state->size && err) {
            auto [newSize, errOp]{ m_policy->Recv(*m_data, std::span<std::byte>{state->buffer}, RecvModeEnum::Any, m_options) };
            err = errOp;

            state->index -= state->size;
            state->size = newSize;
        }

        if (err.has_value())
            return {};

        state->status = err;
        state->buffer[state->size++] = {};

        if (err.error() == ConnectionErrorEnum::ConnectionClosed) {
            CloseSocket(m_data->socket);
            m_data->socket = macroINVALID_SOCKET;
        }

        return state->status;
    }


    // ==============================================================================
    // Transfer Policy Operations (Recv / Send)
    // ==============================================================================

    template<SocketDataConcept Data>
    StreamByteOper TlsTransferPolicy<Data>::Recv(
        Data& data,
        std::span<std::byte> bufferRecv,
        const RecvModeEnum recvMode,
        const RecvOptions options
    ) noexcept {
        size_t totalReceived{}, bytesReceived{};
        ConnectionResultOper err{};

        if (m_streamState != nullptr) {
            const auto size{ std::min(static_cast<size_t>(m_streamState->size - m_streamState->index), bufferRecv.size()) };
            std::memcpy(bufferRecv.data(), m_streamState->buffer.data() + m_streamState->index, size);

            m_streamState->index += size;
            totalReceived += size;
            bufferRecv     = bufferRecv.subspan(size);

            if (bufferRecv.empty())
                return { totalReceived, {} };
        }

                if (!data.transferStateMachine)
            data.transferStateMachine = std::make_unique<details_::TlsTransferStateMachine<Data, TlsTransferPolicy>>();

        details_::ScopedNonBlocking nonBlocking{ data.socket, options.deadline.has_value() };
        data.transferStateMachine->StartToRecv(std::as_writable_bytes(bufferRecv), recvMode, options.deadline);
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
    StreamByteOper TlsTransferPolicy<Data>::Send(
        Data& data,
        std::span<const std::byte> bufferSend,
        const SendOptions options
    ) noexcept {
        if (!data.transferStateMachine)
            data.transferStateMachine = std::make_unique<details_::TlsTransferStateMachine<Data, TlsTransferPolicy>>();

        details_::ScopedNonBlocking nonBlocking{ data.socket, options.deadline.has_value() };
        data.transferStateMachine->StartToSend(std::as_bytes(bufferSend), options.deadline);

        if (!data.transferStateMachine->IsFinished())
            data.transferStateMachine->Advance(data);



        return data.transferStateMachine->GetResult();
    }

}