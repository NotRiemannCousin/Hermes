#pragma once
#include <algorithm>

namespace Hermes {
    template<SocketDataConcept Data>
    template<ByteLike Byte>
    TlsTransferPolicy<Data>::template RecvRange<Byte>::Iterator::value_type TlsTransferPolicy<Data>::RecvRange<Byte>::Iterator::operator*() const {
        if (view->_index == view->_size)
            view->Receive();

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
        return ++(*this);
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    bool TlsTransferPolicy<Data>::RecvRange<Byte>::Iterator::operator==(std::default_sentinel_t) const {
        return !view->_errorStatus.has_value() || (view->_data.socket == macroINVALID_SOCKET);
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
        _index = 0;
        _size = 0;

        auto result = TlsTransferPolicy::Recv(_data, std::span<std::byte>(_buffer));

        if (!result.has_value()) {
            _errorStatus = std::unexpected{ result.error() };
            return _errorStatus;
        }

        _size = static_cast<int>(*result);

        if (_size == 0) {
            _errorStatus = std::unexpected{ ConnectionErrorEnum::CONNECTION_CLOSED };
        }

        return {};
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteCount TlsTransferPolicy<Data>::Recv(Data& data, std::span<Byte> bufferRecv) {
    if (!data.isHandshakeComplete) {
            return std::unexpected{ ConnectionErrorEnum::HANDSHAKE_FAILED };
        }

        if (data.decryptedDataSize > 0) {
            const size_t toCopy = min(bufferRecv.size(), data.decryptedDataSize);
            std::memcpy(bufferRecv.data(), data.decryptedData.data(), toCopy);

            // Move remaining data to front
            if (toCopy < data.decryptedDataSize) {
                std::memmove(data.decryptedData.data(),
                           data.decryptedData.data() + toCopy,
                           data.decryptedDataSize - toCopy);
            }
            data.decryptedDataSize -= toCopy;

            return toCopy;
        }


        SecBuffer buffers[4];
        SecBufferDesc buffDesc{
            macroSECBUFFER_VERSION,
            4,
            buffers
        };

        SECURITY_STATUS status;
        size_t receivedTotal = data.encryptedDataSize;

        do {
            // Receive encrypted data from socket
            if (receivedTotal >= data.encryptedData.size()) {
                return std::unexpected{ ConnectionErrorEnum::RECEIVE_FAILED };
            }

            int received = recv(data.socket,
                              reinterpret_cast<

                              char*>(data.encryptedData.data() + receivedTotal),
                              static_cast<int>(data.encryptedData.size() - receivedTotal),
                              0);

            if (received <= 0) {
                if (received == 0) {
                    return std::unexpected{ ConnectionErrorEnum::CONNECTION_CLOSED };
                }
                return std::unexpected{ ConnectionErrorEnum::RECEIVE_FAILED };
            }

            receivedTotal += received;

            buffers[0] = SecBuffer{ _tul(receivedTotal), SECBUFFER_DATA, data.encryptedData.data() };
            buffers[1] = SecBuffer{ 0, SECBUFFER_EMPTY, nullptr };
            buffers[2] = SecBuffer{ 0, SECBUFFER_EMPTY, nullptr };
            buffers[3] = SecBuffer{ 0, SECBUFFER_EMPTY, nullptr };

            status = DecryptMessage(&data.ctxtHandle, &buffDesc, 0, nullptr);

            if (status == EncryptStatusEnum::ERR_INCOMPLETE_MESSAGE) {
                // Need more data
                continue;
            }

            if (status == EncryptStatusEnum::INFO_CONTEXT_EXPIRED) {
                // Connection closed gracefully
                return std::unexpected{ ConnectionErrorEnum::CONNECTION_CLOSED };
            }

            if (status == EncryptStatusEnum::INFO_RENEGOTIATE)
                return std::unexpected{ ConnectionErrorEnum::RECEIVE_FAILED };

            if (status != EncryptStatusEnum::ERR_OK)
                return std::unexpected{ ConnectionErrorEnum::RECEIVE_FAILED };

            SecBuffer* pDataBuffer  = nullptr;
            SecBuffer* pExtraBuffer = nullptr;

            for (int i = 0; i < 4; ++i) {
                if (buffers[i].BufferType == SECBUFFER_DATA)  pDataBuffer  = &buffers[i];
                if (buffers[i].BufferType == SECBUFFER_EXTRA) pExtraBuffer = &buffers[i];
            }

            if (!pDataBuffer || pDataBuffer->cbBuffer == 0) {
                if (pExtraBuffer && pExtraBuffer->cbBuffer > 0) {
                    std::memmove(data.encryptedData.data(),
                               data.encryptedData.data() + (receivedTotal - pExtraBuffer->cbBuffer),
                               pExtraBuffer->cbBuffer);
                    receivedTotal = pExtraBuffer->cbBuffer;
                    continue;
                }
                return std::unexpected{ ConnectionErrorEnum::RECEIVE_FAILED };
            }

            const size_t toCopy = min(bufferRecv.size(), static_cast<size_t>(pDataBuffer->cbBuffer));

            std::memcpy(bufferRecv.data(), pDataBuffer->pvBuffer, toCopy);

            if (toCopy < pDataBuffer->cbBuffer) {
                data.decryptedDataSize = pDataBuffer->cbBuffer - toCopy;
                std::memcpy(data.decryptedData.data(),
                          static_cast<BYTE*>(pDataBuffer->pvBuffer) + toCopy,
                          data.decryptedDataSize);
            }

            if (pExtraBuffer && pExtraBuffer->cbBuffer > 0) {
                std::memmove(data.encryptedData.data(),
                           data.encryptedData.data() + (receivedTotal - pExtraBuffer->cbBuffer),
                           pExtraBuffer->cbBuffer);
                data.encryptedDataSize = pExtraBuffer->cbBuffer;
            } else {
                data.encryptedDataSize = 0;
            }

            return toCopy;

        } while (status == EncryptStatusEnum::ERR_INCOMPLETE_MESSAGE);

        return std::unexpected{ ConnectionErrorEnum::RECEIVE_FAILED };
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteCount TlsTransferPolicy<Data>::Send(Data& data, std::span<const Byte> bufferSend) {
        if (!data.isHandshakeComplete)
            return std::unexpected{ ConnectionErrorEnum::HANDSHAKE_FAILED };

        const auto& sizes = data.contextStreamSizes;
        size_t totalSent = 0;

        while (totalSent < bufferSend.size()) {
            // Calculate chunk size (respecting TLS max message size)
            const size_t remainingBytes = bufferSend.size() - totalSent;
            const size_t chunkSize = min(remainingBytes, static_cast<size_t>(sizes.cbMaximumMessage));

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
            SECURITY_STATUS status = EncryptMessage(&data.ctxtHandle, 0, &buffDesc, 0);

            if (status != EncryptStatusEnum::ERR_OK) {
                return std::unexpected{ ConnectionErrorEnum::SEND_FAILED };
            }

            // Calculate total encrypted size
            const size_t encryptedSize = buffers[0].cbBuffer +
                                        buffers[1].cbBuffer +
                                        buffers[2].cbBuffer;

            size_t sentBytes = 0;
            while (sentBytes < encryptedSize) {
                int sent = send(data.socket,
                              reinterpret_cast<const char*>(data.encryptedData.data() + sentBytes),
                              static_cast<int>(encryptedSize - sentBytes),
                              0);

                if (sent <= 0) {
                    return std::unexpected{ ConnectionErrorEnum::SEND_FAILED };
                }

                sentBytes += sent;
            }

            totalSent += chunkSize;
        }

        return totalSent;
    }
}
