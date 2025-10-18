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
        auto [newSize, err]{ TlsTransferPolicy::RecvHelper<std::byte>(_data, _buffer, true) };

        _index = 0;
        _size = newSize;

        if (!err.has_value()) {
            _errorStatus = err;

            if (err.error() == ConnectionErrorEnum::CONNECTION_CLOSED) {
                closesocket(_data.socket);
                _data.socket = reinterpret_cast<SOCKET>(nullptr);
            }

            return _errorStatus;
        }

        return {};
    }


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    StreamByteOper TlsTransferPolicy<Data>::RecvHelper(Data& data, std::span<Byte> bufferRecv, bool single) {
        using std::span;
        using std::byte;
        using std::array;
        using std::memmove;
        using std::memcpy;


        if (!data.isHandshakeComplete)
            return { 0, std::unexpected{ ConnectionErrorEnum::HANDSHAKE_FAILED } };

        const span<byte> decryptedData = data.decryptedData;

        span<byte>& dataSpan{ data.decryptedDataSpan };
        span<byte>& extraSpan{ data.decryptedExtraSpan };

        size_t initialSize{ bufferRecv.size() };

        if (!dataSpan.empty()) {
            const size_t countToCopy{ min(bufferRecv.size(), dataSpan.size()) };

            memcpy(bufferRecv.data(), dataSpan.data(), countToCopy);
            bufferRecv = bufferRecv.subspan(countToCopy);

            dataSpan = dataSpan.subspan(countToCopy);
            if (bufferRecv.empty())
                return { countToCopy, {} };
        }


        SECURITY_STATUS status{};

        array<SecBuffer, 4> secBuffers{};
        SecBufferDesc buffDesc = { macroSECBUFFER_VERSION, 4, secBuffers.data() };


        do {
            if (!extraSpan.empty() && extraSpan.data() != decryptedData.data())
                memmove(decryptedData.data(), extraSpan.data(), extraSpan.size());

            dataSpan = decryptedData.subspan(extraSpan.size()); // pula pra nn sobrescrever o extra

            const int received{ recv(
                    data.socket,
                    reinterpret_cast<char*>(dataSpan.data()),
                static_cast<int>(dataSpan.size()),
                0
                ) };

            if (received == 0)
                return { 0, std::unexpected{ ConnectionErrorEnum::CONNECTION_CLOSED} };
            if (received <= 0)
                return { 0, std::unexpected{ ConnectionErrorEnum::RECEIVE_FAILED } };

            dataSpan = decryptedData.first(extraSpan.size() + received); // data pra ser processada, novo + extra


            secBuffers[0] = {
                _tul(dataSpan.size()),
                SECBUFFER_DATA,
                dataSpan.data()
            };

            secBuffers[1] = { 0,SECBUFFER_EMPTY, nullptr };
            secBuffers[2] = { 0,SECBUFFER_EMPTY, nullptr };
            secBuffers[3] = { 0,SECBUFFER_EMPTY, nullptr };

            status = DecryptMessage(&data.ctxtHandle, &buffDesc, 0, nullptr);

            if (status == EncryptStatusEnum::ERR_INCOMPLETE_MESSAGE) {
                extraSpan = dataSpan;
                continue;
            }

            if (status != EncryptStatusEnum::ERR_OK) {
                if (status == EncryptStatusEnum::INFO_CONTEXT_EXPIRED)
                    return { 0, {} };
                if (status == EncryptStatusEnum::INFO_RENEGOTIATE)
                    return { 0, std::unexpected{ ConnectionErrorEnum::SEND_FAILED } };

                return { 0, std::unexpected{ ConnectionErrorEnum::DECRYPTION_FAILED } };
            }

            SecBuffer* pDataBuffer  = nullptr;
            SecBuffer* pExtraBuffer = nullptr;

            for (auto& buffer : secBuffers) {
                if (buffer.BufferType == SecurityBufferEnum::DATA)  pDataBuffer  = &buffer;
                if (buffer.BufferType == SecurityBufferEnum::EXTRA) pExtraBuffer = &buffer;
            }

            if (pExtraBuffer && pExtraBuffer->cbBuffer > 0)
                extraSpan = { static_cast<byte*>(pExtraBuffer->pvBuffer), pExtraBuffer->cbBuffer };
            else
                extraSpan = {};

            if (pDataBuffer && pDataBuffer->cbBuffer > 0) {
                dataSpan = { static_cast<byte*>(pDataBuffer->pvBuffer), pDataBuffer->cbBuffer };
                const size_t countToCopy{ min(bufferRecv.size(), pDataBuffer->cbBuffer) };
                memcpy(bufferRecv.data(), pDataBuffer->pvBuffer, countToCopy);

                bufferRecv = bufferRecv.subspan(countToCopy);
                dataSpan = dataSpan.subspan(countToCopy);

                if (bufferRecv.empty())
                    break; // se encher acaba
            }

            if (!pExtraBuffer && !pDataBuffer)
                return { initialSize - bufferRecv.size(), std::unexpected{ ConnectionErrorEnum::CONNECTION_CLOSED } };

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
            return { 0, std::unexpected{ ConnectionErrorEnum::HANDSHAKE_FAILED } };

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
                return { 0, std::unexpected{ ConnectionErrorEnum::SEND_FAILED } };
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
                    return { totalSent, std::unexpected{ ConnectionErrorEnum::SEND_FAILED } };
                }

                sentBytes += sent;
            }

            totalSent += chunkSize;
        }

        return { 0, {} };
    }
}
