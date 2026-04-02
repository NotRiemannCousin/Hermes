#pragma once
#include <algorithm>
// EU ODEIO TLS NAMORAL

namespace Hermes {
    template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::template RecvRange<Byte>::Iterator::value_type TlsTransferPolicy<Data>::RecvRange<Byte>::Iterator::operator*() const {
        if (view->_index >= view->_size)
            auto _{ view->Receive() };

        return std::bit_cast<Byte>(view->_buffer[view->_index]);
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::template RecvRange<Byte>::Iterator& TlsTransferPolicy<Data>::RecvRange<Byte>::Iterator::operator++() {
        ++view->_index;
        return *this;
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::template RecvRange<Byte>::Iterator& TlsTransferPolicy<Data>::RecvRange<Byte>::Iterator::operator++(int) {
        return ++*this;
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    bool TlsTransferPolicy<Data>::RecvRange<Byte>::Iterator::operator==(std::default_sentinel_t) const {
        return (!view->_errorStatus && view->_index == view->_size)
                || view->_data.socket == macroINVALID_SOCKET;
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::RecvRange<Byte>::RecvRange(Data& data, TlsTransferPolicy&) : _data { data } { }



    template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::template RecvRange<Byte>::Iterator TlsTransferPolicy<Data>::RecvRange<Byte>::begin() {
        return Iterator{this};
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    std::default_sentinel_t TlsTransferPolicy<Data>::RecvRange<Byte>::end() { return {}; }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    ConnectionResultOper TlsTransferPolicy<Data>::RecvRange<Byte>::Error() const {
        return _errorStatus;
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    ConnectionResultOper TlsTransferPolicy<Data>::RecvRange<Byte>::Receive() {
        StreamByteOper::second_type err{};
        while (_index >= _size && err) {
            auto [newSize, errOp]{ TlsTransferPolicy::RecvHelper<std::byte>(_data, _buffer, true) };
            err = errOp;

            _index -= _size;
            _size = newSize;
        }

        if (err.has_value())
            return {};

        _errorStatus = err;

        if (err.error() == ConnectionErrorEnum::ConnectionClosed) {
            closesocket(_data.socket);
            _data.socket = reinterpret_cast<SOCKET>(nullptr);
        }

        return _errorStatus;
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteOper TlsTransferPolicy<Data>::RecvHelper(Data& data, std::span<Byte> bufferRecv, const bool single) {
        using std::span;
        using std::byte;
        using std::array;
        using std::memmove;
        using std::memcpy;


        if (!data.isHandshakeComplete)
            return { 0, std::unexpected{ ConnectionErrorEnum::HandshakeFailed } };

        const span<byte> decryptedData{ data.decryptedData };

        span<byte>& dataSpan{ data.decryptedDataSpan };
        span<byte>& extraSpan{ data.decryptedExtraSpan };

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
                        using std::ranges::find;
                        const auto dataBuffer {
                            find(secBuffers, _tul(SecurityBufferEnum::Data), &SecBuffer::BufferType)
                        };
                        const auto extraBuffer{
                            find(secBuffers, _tul(SecurityBufferEnum::Extra), &SecBuffer::BufferType)
                        };

                        if (dataBuffer == secBuffers.end())
                            return { 0, std::unexpected{ ConnectionErrorEnum::Unknown } };

                        dataSpan = { static_cast<byte*>(dataBuffer->pvBuffer), dataBuffer->cbBuffer };

                        if (extraBuffer != secBuffers.end() && HasData(*extraBuffer))
                            extraSpan = { static_cast<byte*>(extraBuffer->pvBuffer), extraBuffer->cbBuffer };
                        else
                            extraSpan = { decryptedData.data(), 0 };

                        if (!HasData(*dataBuffer))
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
                    using std::ranges::find;
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
                case EncryptStatusEnum::InfoRenegotiate:
                    // TODO: Make a new Handshake
                    return { 0, std::unexpected{ ConnectionErrorEnum::DecryptionFailed } };
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
        return RecvHelper(data,  bufferRecv, false);
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
            buffers[0] = SecBuffer{
                sizes.cbHeader,
                SECBUFFER_STREAM_HEADER,
                static_cast<void*>(data.encryptedData.data())
            };

            buffers[1] = SecBuffer{
                static_cast<ULONG>(chunkSize),
                SECBUFFER_DATA,
                static_cast<void*>(data.encryptedData.data() + sizes.cbHeader)
            };

            buffers[2] = SecBuffer{
                sizes.cbTrailer,
                SECBUFFER_STREAM_TRAILER,
                static_cast<void*>(data.encryptedData.data() + sizes.cbHeader + chunkSize)
            };

            buffers[3] = SecBuffer{
                0,
                SECBUFFER_EMPTY,
                nullptr
            };

            std::memcpy(buffers[1].pvBuffer,
                       bufferSend.data() + totalSent,
                       chunkSize);

            SecBufferDesc buffDesc{
                SECBUFFER_VERSION,
                4,
                buffers
            };

            // Encrypt the message
            // ReSharper disable once CppTooWideScopeInitStatement
            const SECURITY_STATUS status{ EncryptMessage(&data.ctxtHandle, 0, &buffDesc, 0) };

            if (status != EncryptStatusEnum::ErrOk) {
                return { 0, std::unexpected{ ConnectionErrorEnum::SendFailed } };
            }

            // Calculate total encrypted size
            const size_t encryptedSize{ buffers[0].cbBuffer +
                                        buffers[1].cbBuffer +
                                        buffers[2].cbBuffer };

            size_t sentBytes{};
            while (sentBytes < encryptedSize) {
                const int sent{ send(data.socket,
                              reinterpret_cast<const char*>(data.encryptedData.data() + sentBytes),
                              static_cast<int>(encryptedSize - sentBytes),
                              0) };

                if (sent <= 0)
                    return { totalSent, std::unexpected{ ConnectionErrorEnum::SendFailed } };

                sentBytes += sent;
            }

            totalSent += chunkSize;
        }

        return { 0, {} };
    }
}
