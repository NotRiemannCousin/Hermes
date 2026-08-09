#pragma once
#include <print>

namespace Hermes {
    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    Byte DefaultTransferPolicy<SocketType>::RecvStream<Byte>::Iterator::operator*() const {
        if (view->m_policy->m_state->index >= view->m_policy->m_state->size)
            auto _{ view->Receive() };
        return std::bit_cast<Byte>(view->m_policy->m_state->buffer[view->m_policy->m_state->index]);
    }

    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    auto DefaultTransferPolicy<SocketType>::RecvStream<Byte>::Iterator::operator++() -> Iterator& {
        ++view->m_policy->m_state->index;
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
        return (!view->m_policy->m_state->status && view->m_policy->m_state->index >= view->m_policy->m_state->size)
                || *view->m_socket == macroINVALID_SOCKET;
    }


    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    template<SocketDataConcept Data>
    DefaultTransferPolicy<SocketType>::RecvStream<Byte>::RecvStream(Data& data, DefaultTransferPolicy& policy)
        : m_socket{ &data.socket }, m_policy{ &policy } {
        if (policy.m_state == nullptr)
            policy.m_state = std::make_unique<State>();
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
        return m_policy->m_state->status;
    }


    template<SocketTypeEnum SocketType>
    template<ByteLike Byte>
    ConnectionResultOper DefaultTransferPolicy<SocketType>::RecvStream<Byte>::Receive() {
        StreamByteOper::second_type err{};
        auto& state{ m_policy->m_state };

        while (state->index >= state->size && err) {
            auto [newSize, errOp]{ DefaultTransferPolicy::RecvHelper(*m_socket, state->buffer, RecvModeEnum::Any) };
            err = errOp;

            state->index -= state->size;
            state->size = newSize;
        }

        if (err.has_value())
            return {};
        state->status = err;
        state->buffer[state->size++] = {};

        if (err.error() == ConnectionErrorEnum::ConnectionClosed) {
            CloseSocket(*m_socket);
            *m_socket = macroINVALID_SOCKET;
        }

        return state->status;
    }


    template<SocketTypeEnum SocketType>
    template<SocketDataConcept Data>
    StreamByteOper DefaultTransferPolicy<SocketType>::Recv(Data& data, std::span<std::byte> bufferRecv, const RecvModeEnum recvMode) {
        if (m_state != nullptr) {
            const auto size{ std::min((size_t)m_state->size - m_state->index, bufferRecv.size()) };
            std::memcpy(bufferRecv.data(), m_state->buffer.data() + m_state->index, size);
            m_state->index += size;

            bufferRecv = bufferRecv.subspan(size);
            if (bufferRecv.empty())
                return { size, {} };
        }

        return DefaultTransferPolicy::RecvHelper(data.socket, bufferRecv, recvMode);
    }

    template<SocketTypeEnum SocketType>
    StreamByteOper DefaultTransferPolicy<SocketType>::RecvHelper(SocketFd& socket, std::span<std::byte> bufferRecv, const RecvModeEnum recvMode) {
        if (socket == macroINVALID_SOCKET)
            return {0, std::unexpected{ ConnectionErrorEnum::SocketNotOpen } };
        size_t total{};
        do {
            const IoCount received{ recv(socket,
                reinterpret_cast<char*>(bufferRecv.data() + total),
                static_cast<int>(bufferRecv.size() - total), 0) };
            if (received == 0) {
                CloseSocket(std::exchange(socket, macroINVALID_SOCKET));
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
    template<SocketDataConcept Data>
    StreamByteOper DefaultTransferPolicy<SocketType>::Send(Data& data, std::span<const std::byte> bufferSend) {
        if (data.socket == macroINVALID_SOCKET)
            return { 0, std::unexpected{ ConnectionErrorEnum::SocketNotOpen } };
        size_t total{};
        while (total < bufferSend.size()) {
            const IoCount sent{ send(data.socket,
                reinterpret_cast<const char*>(bufferSend.data() + total),
                static_cast<int>(bufferSend.size() - total), 0) };
            if (sent == macroSOCKET_ERROR)
                return { total, std::unexpected{ ConnectionErrorEnum::SendFailed } };
            total += sent;
        }
        return { total, {} };
    }

}