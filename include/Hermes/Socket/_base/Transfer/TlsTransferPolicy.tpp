#pragma once
#include <algorithm>
// EU ODEIO TLS NAMORAL

namespace Hermes {
    template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::template RecvLazyRange<Byte>::Iterator::value_type TlsTransferPolicy<Data>::RecvLazyRange<Byte>::Iterator::operator*() const {
        if (view->_policy->_state->index >= view->_policy->_state->size)
            auto _{ view->Receive() };

        return std::bit_cast<Byte>(view->_policy->_state->buffer[view->_policy->_state->index]);
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::template RecvLazyRange<Byte>::Iterator& TlsTransferPolicy<Data>::RecvLazyRange<Byte>::Iterator::operator++() {
        ++view->_policy->_state->index;
        return *this;
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::template RecvLazyRange<Byte>::Iterator& TlsTransferPolicy<Data>::RecvLazyRange<Byte>::Iterator::operator++(int) {
        return ++*this;
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    bool TlsTransferPolicy<Data>::RecvLazyRange<Byte>::Iterator::operator==(std::default_sentinel_t) const {
        const auto& state{ view->_policy->_state };

        return (!state->status && state->index >= state->size)
                || view->_data->socket == macroINVALID_SOCKET;
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::RecvLazyRange<Byte>::RecvLazyRange(Data& data, TlsTransferPolicy& policy)
        : _data{ &data }, _policy{ &policy } {
        if (policy._state == nullptr)
            policy._state = std::make_unique<State>();
    }



    template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::template RecvLazyRange<Byte>::Iterator TlsTransferPolicy<Data>::RecvLazyRange<Byte>::begin() {
        return Iterator{ this };
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    std::default_sentinel_t TlsTransferPolicy<Data>::RecvLazyRange<Byte>::end() { return {}; }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    ConnectionResultOper TlsTransferPolicy<Data>::RecvLazyRange<Byte>::Error() const {
        return _policy->_state->status;
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    ConnectionResultOper TlsTransferPolicy<Data>::RecvLazyRange<Byte>::Receive() {
        StreamByteOper::second_type err{};
        auto& state{ _policy->_state };

        while (state->index >= state->size && err) {
            auto [newSize, errOp]{ TlsTransferPolicy::RecvHelper<std::byte>(*_data, state->buffer, true) };
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
    StreamByteOper TlsTransferPolicy<Data>::RecvHelper(Data& data, std::span<Byte> bufferRecv, const bool single) {
        static constexpr auto s_hasData = [](const SecBuffer buffer) {
            return buffer.cbBuffer > 0;
        };

        using std::ranges::find;
        using std::span;
        using std::byte;
        using std::array;
        using std::memmove;
        using std::memcpy;


        if (!data.isHandshakeComplete)
            return { 0, std::unexpected{ ConnectionErrorEnum::HandshakeFailed } };

        const span<byte> decryptedData{ data.state->decryptedData };

        span<byte>& dataSpan{ data.state->decryptedDataSpan };
        span<byte>& extraSpan{ data.state->decryptedExtraSpan };

        size_t initialSize{ bufferRecv.size() };

        SECURITY_STATUS status{};

        array<SecBuffer, 4> secBuffers{};
        SecBufferDesc buffDesc{ macroSECBUFFER_VERSION, 4, secBuffers.data() };

        do {
            GT_TRY_AGAIN:

            const bool hasPendingData{ !dataSpan.empty() };
            const bool hasPendingExtraData{ !extraSpan.empty() };

            if (hasPendingData)
                status = _tul(EncryptStatusEnum::ErrOk);
            else {
                if (hasPendingExtraData && status != EncryptStatusEnum::ErrIncompleteMessage) {
                    dataSpan = extraSpan;
                }
                else {
                    const int received{ recv(
                            data.socket,
                            reinterpret_cast<char*>(decryptedData.data() + extraSpan.size()),
                        static_cast<int>(decryptedData.size() - extraSpan.size()),
                        0
                        ) };


                    if (received == 0)
                        return { 0, std::unexpected{ ConnectionErrorEnum::ConnectionClosed} };
                    if (received < 0)
                        return { 0, std::unexpected{ ConnectionErrorEnum::ReceiveFailed } };

                    dataSpan = decryptedData.first(extraSpan.size() + received);
                }


                secBuffers[0] = { _tul(dataSpan.size()), _tul(SecurityBufferEnum::Data), dataSpan.data() };
                secBuffers[1] = secBuffers[2] = secBuffers[3] = { 0, _tul(SecurityBufferEnum::Empty), nullptr };

                status = DecryptMessage(&data.ctxtHandle, &buffDesc, 0, nullptr);
            }

            // All values listed in
            // https://learn.microsoft.com/pt-br/windows/win32/api/sspi/nf-sspi-decryptmessage
            // and
            // https://learn.microsoft.com/en-us/windows/win32/secauthn/decryptmessage--schannel
            switch (static_cast<EncryptStatusEnum>(status)) {
                case EncryptStatusEnum::ErrOk: {
                    if (!hasPendingData) {
                        const auto dataBuffer {
                            find(secBuffers, _tul(SecurityBufferEnum::Data), &SecBuffer::BufferType)
                        };
                        const auto extraBuffer{
                            find(secBuffers, _tul(SecurityBufferEnum::Extra), &SecBuffer::BufferType)
                        };

                        if (dataBuffer == secBuffers.end())
                            return { 0, std::unexpected{ ConnectionErrorEnum::Unknown } };

                        dataSpan = { static_cast<byte*>(dataBuffer->pvBuffer), dataBuffer->cbBuffer };

                        if (extraBuffer != secBuffers.end() && s_hasData(*extraBuffer))
                            extraSpan = { static_cast<byte*>(extraBuffer->pvBuffer), extraBuffer->cbBuffer };
                        else
                            extraSpan = { decryptedData.data(), 0 };

                        if (!s_hasData(*dataBuffer))
                            continue;
                    }

                    const size_t countToCopy{ min(bufferRecv.size(), dataSpan.size()) };
                    std::memmove(bufferRecv.data(), dataSpan.data(), countToCopy);
                    std::memmove(decryptedData.data(), extraSpan.data(), extraSpan.size());

                    extraSpan = { decryptedData.data(), extraSpan.size() };

                    bufferRecv = bufferRecv.subspan(countToCopy);
                    dataSpan   = dataSpan.subspan(countToCopy);

                    if (bufferRecv.empty())
                        return { initialSize - bufferRecv.size(), {} };
                    break;
                }
                case EncryptStatusEnum::ErrIncompleteMessage: {
                    const auto extraBuffer{
                        find(secBuffers, _tul(SecurityBufferEnum::Extra), &SecBuffer::BufferType)
                    };
                    const auto missingBuffer{
                        find(secBuffers, _tul(SecurityBufferEnum::Missing), &SecBuffer::BufferType)
                    };

                    if (extraBuffer == secBuffers.end()) {
                        if (missingBuffer == secBuffers.end())
                            return { 0, std::unexpected{ ConnectionErrorEnum::Unknown } };

                        extraSpan = dataSpan;
                    } else {
                        extraSpan = { static_cast<byte*>(extraBuffer->pvBuffer), extraBuffer->cbBuffer };
                    }
                    std::memmove(decryptedData.data(), extraSpan.data(), extraSpan.size());

                    dataSpan  = {};
                    extraSpan = decryptedData.first(extraSpan.size());
                    goto GT_TRY_AGAIN;
                }
                case EncryptStatusEnum::InfoRenegotiate: {
                    data.isHandshakeComplete = false;

                    // 1. Procurar por dados extras (SECBUFFER_EXTRA) deixados pelo DecryptMessage
                    const auto extraBuffer{
                        find(secBuffers, _tul(SecurityBufferEnum::Extra), &SecBuffer::BufferType)
                    };

                    if (extraBuffer != secBuffers.end() && s_hasData(*extraBuffer)) {
                        extraSpan = { static_cast<std::byte*>(extraBuffer->pvBuffer), extraBuffer->cbBuffer };
                    } else {
                        extraSpan = {}; // O SSPI consumiu tudo, não sobrou nada
                    }

                    // 2. Move os dados residuais para o início (se existirem)
                    if (!extraSpan.empty()) {
                        std::memmove(decryptedData.data(), extraSpan.data(), extraSpan.size());
                    }

                    // 3. Avisa o Handshake
                    data.pendingData = static_cast<uint32_t>(extraSpan.size());

                    if (!data.handshakeCallback)
                        return { 0, std::unexpected{ ConnectionErrorEnum::DecryptionFailed } };

                    const auto hsResult{ data.handshakeCallback(data) };
                    if (!hsResult)
                        return { 0, std::unexpected{ hsResult.error() } };

                    // Limpa apenas o dataSpan. O extraSpan pode ter sido preenchido pelo
                    // Handshake com os novos dados de aplicação que chegaram!
                    dataSpan = {};
                    goto GT_TRY_AGAIN;
                }
                case EncryptStatusEnum::ErrBufferTooSmall:
                case EncryptStatusEnum::ErrCryptoSystemInvalid:
                case EncryptStatusEnum::ErrQopNotSupported:
                    return { 0, std::unexpected{ ConnectionErrorEnum::DecryptionFailed } };
                case EncryptStatusEnum::InfoContextExpired:
                    return { 0, std::unexpected{ ConnectionErrorEnum::ConnectionClosed } };
                case EncryptStatusEnum::ErrInvalidHandle:
                case EncryptStatusEnum::ErrInvalidToken:
                case EncryptStatusEnum::ErrMessageAltered:
                case EncryptStatusEnum::ErrOutOfSequence:
                case EncryptStatusEnum::ErrDecryptFailure:
                default:
                    return { 0, std::unexpected{ ConnectionErrorEnum::DecryptionFailed } };
            }
        } while (!single);

        return { initialSize - bufferRecv.size(), {} };
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteOper TlsTransferPolicy<Data>::Recv(Data& data, std::span<Byte> bufferRecv) {
        if (_state != nullptr) {
            const auto size{ min(_state->size - _state->index, bufferRecv.size()) };

            std::memcpy(bufferRecv.data(), _state->buffer.data() + _state->index, size);
            _state->index += size;

            bufferRecv = bufferRecv.subspan(size);
            if (bufferRecv.empty())
                return { size, {} };
        }

        return TlsTransferPolicy::RecvHelper(data, bufferRecv, false);
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteOper TlsTransferPolicy<Data>::Send(Data& data, std::span<const Byte> bufferSend) {
        if (!data.isHandshakeComplete)
            return { 0, std::unexpected{ ConnectionErrorEnum::HandshakeFailed } };

        const auto& sizes{ data.contextStreamSizes };
        size_t totalSent{};

        while (totalSent < bufferSend.size()) {
            // Calculate chunk size (respecting TLS max message size)
            const size_t remainingBytes{ bufferSend.size() - totalSent };
            const size_t chunkSize{ min(remainingBytes, static_cast<size_t>(sizes.cbMaximumMessage)) };

            // Setup encryption buffers
            SecBuffer buffers[4];

            // Header buffer

            const auto dwChunkSize{ static_cast<ULONG>(chunkSize) };

            buffers[0] = SecBuffer{ sizes.cbHeader , _tul(SecurityBufferEnum::StreamHeader) ,
                    static_cast<void*>(data.state->encryptedData.data()) };
            buffers[1] = SecBuffer{ dwChunkSize    , _tul(SecurityBufferEnum::Data)         ,
                    static_cast<void*>(data.state->encryptedData.data() + sizes.cbHeader) };
            buffers[2] = SecBuffer{ sizes.cbTrailer, _tul(SecurityBufferEnum::StreamTrailer),
                    static_cast<void*>(data.state->encryptedData.data() + sizes.cbHeader + chunkSize) };
            buffers[3] = SecBuffer{ 0              , _tul(SecurityBufferEnum::Empty)        ,
                    nullptr };

            std::memcpy(buffers[1].pvBuffer, bufferSend.data() + totalSent, chunkSize);

            SecBufferDesc buffDesc{ macroSECBUFFER_VERSION, 4, buffers };

            // Encrypt the message
            // ReSharper disable once CppTooWideScopeInitStatement
            const SECURITY_STATUS status{ EncryptMessage(&data.ctxtHandle, 0, &buffDesc, 0) };

            if (status != EncryptStatusEnum::ErrOk) {
                if (static_cast<EncryptStatusEnum>(status) == EncryptStatusEnum::InfoContextExpired)
                    return { totalSent, std::unexpected{ ConnectionErrorEnum::ConnectionClosed } };
                return { totalSent, std::unexpected{ ConnectionErrorEnum::SendFailed } };
            }

            // Calculate total encrypted size
            const size_t encryptedSize{ buffers[0].cbBuffer +
                                        buffers[1].cbBuffer +
                                        buffers[2].cbBuffer };

            size_t sentBytes{};
            while (sentBytes < encryptedSize) {
                const int sent{ send(data.socket,
                              reinterpret_cast<const char*>(data.state->encryptedData.data() + sentBytes),
                              static_cast<int>(encryptedSize - sentBytes),
                              0) };

                if (sent <= 0)
                    return { totalSent, std::unexpected{ ConnectionErrorEnum::SendFailed } };

                sentBytes += static_cast<size_t>(sent);
            }

            totalSent += chunkSize;
        }

        return { totalSent, {} };
    }
}
