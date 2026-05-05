#pragma once
#include <algorithm>

namespace Hermes {

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    struct TlsAsyncTransferPolicy<Data>::RecvSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(size_t),
            stdexec::set_error_t(TransferError),
            stdexec::set_stopped_t()
        >;

        Data* _data;
        std::span<Byte> _buffer;
        RecvModeEnum _mode;

        template<class Receiver>
        struct OperationState {
            Data* _data;
            std::span<Byte> _buffer;
            RecvModeEnum _mode;
            Receiver _receiver;

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                Data& data = *self._data;
                std::span<Byte> bufferRecv = self._buffer;
                const RecvModeEnum recvMode = self._mode;

                static constexpr auto s_hasData = [](const SecBuffer buffer) {
                    return buffer.cbBuffer > 0;
                };

                const auto s_asyncRecv = [&](char* buf, int len) {
                    fd_set fd; FD_ZERO(&fd); FD_SET(data.socket, &fd);
                    select(static_cast<int>(data.socket) + 1, &fd, nullptr, nullptr, nullptr);
                    return recv(data.socket, buf, len, 0);
                };

                using std::ranges::find;
                using std::span;
                using std::byte;
                using std::array;

                if (!data.isHandshakeComplete) {
                    stdexec::set_error(std::move(self._receiver), TransferError{ 0, ConnectionErrorEnum::HandshakeFailed });
                    return;
                }

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
                            const int received{ s_asyncRecv(
                                reinterpret_cast<char*>(decryptedData.data() + extraSpan.size()),
                                static_cast<int>(decryptedData.size() - extraSpan.size())
                            ) };

                            if (received == 0) {
                                stdexec::set_error(std::move(self._receiver), TransferError{ initialSize - bufferRecv.size(), ConnectionErrorEnum::ConnectionClosed });
                                return;
                            }
                            if (received < 0) {
                                stdexec::set_error(std::move(self._receiver), TransferError{ initialSize - bufferRecv.size(), ConnectionErrorEnum::ReceiveFailed });
                                return;
                            }

                            dataSpan = decryptedData.first(extraSpan.size() + received);
                            extraSpan = {};
                        }

                        secBuffers[0] = { _tul(dataSpan.size()), _tul(SecurityBufferEnum::Data), dataSpan.data() };
                        secBuffers[1] = secBuffers[2] = secBuffers[3] = { 0, _tul(SecurityBufferEnum::Empty), nullptr };

                        status = DecryptMessage(&data.ctxtHandle, &buffDesc, 0, nullptr);
                    }

                    switch (static_cast<EncryptStatusEnum>(status)) {
                        case EncryptStatusEnum::ErrOk: {
                            if (!hasPendingData) {
                                const auto dataBuffer { find(secBuffers, _tul(SecurityBufferEnum::Data), &SecBuffer::BufferType) };
                                const auto extraBuffer{ find(secBuffers, _tul(SecurityBufferEnum::Extra), &SecBuffer::BufferType) };

                                if (dataBuffer == secBuffers.end()) {
                                    stdexec::set_error(std::move(self._receiver), TransferError{ initialSize - bufferRecv.size(), ConnectionErrorEnum::Unknown });
                                    return;
                                }

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

                            bufferRecv = bufferRecv.subspan(countToCopy);
                            dataSpan   = dataSpan.subspan(countToCopy);

                            if (dataSpan.empty() && !extraSpan.empty()) {
                                std::memmove(decryptedData.data(), extraSpan.data(), extraSpan.size());
                                extraSpan = { decryptedData.data(), extraSpan.size() };
                            }

                            if (bufferRecv.empty()) {
                                stdexec::set_value(std::move(self._receiver), initialSize - bufferRecv.size());
                                return;
                            }
                            break;
                        }
                        case EncryptStatusEnum::ErrIncompleteMessage: {
                            const auto extraBuffer{ find(secBuffers, _tul(SecurityBufferEnum::Extra), &SecBuffer::BufferType) };
                            const auto missingBuffer{ find(secBuffers, _tul(SecurityBufferEnum::Missing), &SecBuffer::BufferType) };

                            if (extraBuffer == secBuffers.end()) {
                                if (missingBuffer == secBuffers.end()) {
                                    stdexec::set_error(std::move(self._receiver), TransferError{ initialSize - bufferRecv.size(), ConnectionErrorEnum::Unknown });
                                    return;
                                }
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

                            const auto extraBuffer{ find(secBuffers, _tul(SecurityBufferEnum::Extra), &SecBuffer::BufferType) };

                            if (extraBuffer != secBuffers.end() && s_hasData(*extraBuffer))
                                extraSpan = { static_cast<std::byte*>(extraBuffer->pvBuffer), extraBuffer->cbBuffer };
                            else
                                extraSpan = {};

                            if (!extraSpan.empty()) {
                                std::memmove(decryptedData.data(), extraSpan.data(), extraSpan.size());
                                extraSpan = { decryptedData.data(), extraSpan.size() };
                            }

                            data.pendingData = static_cast<uint32_t>(extraSpan.size());

                            if (!data.handshakeCallback) {
                                stdexec::set_error(std::move(self._receiver), TransferError{ initialSize - bufferRecv.size(), ConnectionErrorEnum::DecryptionFailed });
                                return;
                            }

                            const auto hsResult{ data.handshakeCallback(data) };
                            if (!hsResult) {
                                stdexec::set_error(std::move(self._receiver), TransferError{ initialSize - bufferRecv.size(), hsResult.error() });
                                return;
                            }

                            dataSpan = {};
                            goto GT_TRY_AGAIN;
                        }
                        case EncryptStatusEnum::ErrBufferTooSmall:
                        case EncryptStatusEnum::ErrCryptoSystemInvalid:
                        case EncryptStatusEnum::ErrQopNotSupported:
                            stdexec::set_error(std::move(self._receiver), TransferError{ initialSize - bufferRecv.size(), ConnectionErrorEnum::DecryptionFailed });
                            return;
                        case EncryptStatusEnum::InfoContextExpired:
                            stdexec::set_value(std::move(self._receiver), initialSize - bufferRecv.size());
                            return;
                        case EncryptStatusEnum::ErrInvalidHandle:
                        case EncryptStatusEnum::ErrInvalidToken:
                        case EncryptStatusEnum::ErrMessageAltered:
                        case EncryptStatusEnum::ErrOutOfSequence:
                        case EncryptStatusEnum::ErrDecryptFailure:
                        default:
                            stdexec::set_error(std::move(self._receiver), TransferError{ initialSize - bufferRecv.size(), ConnectionErrorEnum::DecryptionFailed });
                            return;
                    }
                } while (recvMode == RecvModeEnum::All);

                stdexec::set_value(std::move(self._receiver), initialSize - bufferRecv.size());
            }
        };

        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const RecvSender& self, Receiver r) {
            return { self._data, self._buffer, self._mode, std::move(r) };
        }
    };

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    struct TlsAsyncTransferPolicy<Data>::SendSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(size_t),
            stdexec::set_error_t(TransferError),
            stdexec::set_stopped_t()
        >;

        Data* _data;
        std::span<const Byte> _buffer;

        template<class Receiver>
        struct OperationState {
            Data* _data;
            std::span<const Byte> _buffer;
            Receiver _receiver;

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                Data& data = *self._data;
                const std::span<const Byte> bufferSend = self._buffer;

                const auto s_asyncSend = [&](const char* buf, int len) {
                    fd_set fd; FD_ZERO(&fd); FD_SET(data.socket, &fd);
                    select(static_cast<int>(data.socket) + 1, nullptr, &fd, nullptr, nullptr);
                    return send(data.socket, buf, len, 0);
                };

                if (!data.isHandshakeComplete) {
                    stdexec::set_error(std::move(self._receiver), TransferError{ 0, ConnectionErrorEnum::HandshakeFailed });
                    return;
                }

                const auto& sizes{ data.contextStreamSizes };
                size_t totalSent{};

                while (totalSent < bufferSend.size()) {
                    const size_t remainingBytes{ bufferSend.size() - totalSent };
                    const size_t chunkSize{ min(remainingBytes, static_cast<size_t>(sizes.cbMaximumMessage)) };

                    SecBuffer buffers[4];
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

                    const SECURITY_STATUS status{ EncryptMessage(&data.ctxtHandle, 0, &buffDesc, 0) };

                    if (status != EncryptStatusEnum::ErrOk) {
                        if (static_cast<EncryptStatusEnum>(status) == EncryptStatusEnum::InfoContextExpired) {
                            stdexec::set_error(std::move(self._receiver), TransferError{ totalSent, ConnectionErrorEnum::ConnectionClosed });
                            return;
                        }
                        stdexec::set_error(std::move(self._receiver), TransferError{ totalSent, ConnectionErrorEnum::SendFailed });
                        return;
                    }

                    const size_t encryptedSize{ buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer };
                    size_t sentBytes{};

                    while (sentBytes < encryptedSize) {
                        const int sent{ s_asyncSend(
                            reinterpret_cast<const char*>(data.state->encryptedData.data() + sentBytes),
                            static_cast<int>(encryptedSize - sentBytes)
                        ) };

                        if (sent <= 0) {
                            stdexec::set_error(std::move(self._receiver), TransferError{ totalSent, ConnectionErrorEnum::SendFailed });
                            return;
                        }

                        sentBytes += static_cast<size_t>(sent);
                    }

                    totalSent += chunkSize;
                }

                stdexec::set_value(std::move(self._receiver), totalSent);
            }
        };

        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const SendSender& self, Receiver r) {
            return { self._data, self._buffer, std::move(r) };
        }
    };


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto TlsAsyncTransferPolicy<Data>::AsyncRecv(Data& data, std::span<Byte> bufferRecv, RecvModeEnum recvMode) noexcept {
        return RecvSender<Byte>{ &data, bufferRecv, recvMode };
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto TlsAsyncTransferPolicy<Data>::AsyncSend(Data& data, std::span<const Byte> bufferSend) noexcept {
        return SendSender<Byte>{ &data, bufferSend };
    }
}