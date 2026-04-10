#pragma once
#include <format>

namespace std {
    template <>
    struct formatter<Hermes::ConnectionErrorEnum> {
        bool isVerbose = false;

        constexpr auto parse(std::format_parse_context& ctx) {
            auto it = ctx.begin();
            if (it != ctx.end() && *it == 'v') {
                isVerbose = true;
                ++it;
            }
            if (it != ctx.end() && *it != '}')
                throw std::format_error("Invalid option for ConnectionErrorEnum");

            return it;
        }

        template<class FormatContext>
        auto format(const Hermes::ConnectionErrorEnum error, FormatContext &ctx) const {
            if (isVerbose) {
                switch (error) {
                    case Hermes::ConnectionErrorEnum::Unknown:
                        return std::format_to(ctx.out(), "Unknown error: An unidentified error occurred.");
                    case Hermes::ConnectionErrorEnum::InvalidRole:
                        return std::format_to(ctx.out(), "Invalid role: Network role (client/server) is invalid.");
                    case Hermes::ConnectionErrorEnum::InvalidEndpoint:
                        return std::format_to(ctx.out(), "Invalid endpoint: IP address or port is malformed.");
                    case Hermes::ConnectionErrorEnum::AddressInUse:
                        return std::format_to(ctx.out(), "Address in use: Local IP and port already bound.");
                    case Hermes::ConnectionErrorEnum::SocketNotOpen:
                        return std::format_to(ctx.out(), "Socket not open: Operation on an uninitialized socket.");
                    case Hermes::ConnectionErrorEnum::ConnectionFailed:
                        return std::format_to(ctx.out(), "Connection failed: Attempt to establish connection failed.");
                    case Hermes::ConnectionErrorEnum::ConnectionClosed:
                        return std::format_to(ctx.out(), "Connection closed: Connection gracefully closed by peer.");
                    case Hermes::ConnectionErrorEnum::Interrupted:
                        return std::format_to(ctx.out(), "Interrupted: Blocking operation interrupted by signal.");
                    case Hermes::ConnectionErrorEnum::ConnectionTimeout:
                        return std::format_to(ctx.out(), "Connection timeout: Connection attempt timed out.");
                    case Hermes::ConnectionErrorEnum::SendTimeout:
                        return std::format_to(ctx.out(), "Send timeout: Socket timed out sending data.");
                    case Hermes::ConnectionErrorEnum::ReceiveTimeout:
                        return std::format_to(ctx.out(), "Receive timeout: Socket timed out receiving data.");
                    case Hermes::ConnectionErrorEnum::ListenFailed:
                        return std::format_to(ctx.out(), "Listen failed: Server failed to enter listening state.");
                    case Hermes::ConnectionErrorEnum::SendFailed:
                        return std::format_to(ctx.out(), "Send failed: Error transmitting data to peer.");
                    case Hermes::ConnectionErrorEnum::ReceiveFailed:
                        return std::format_to(ctx.out(), "Receive failed: Error reading data from socket.");
                    case Hermes::ConnectionErrorEnum::HandshakeFailed:
                        return std::format_to(ctx.out(), "Handshake failed: Protocol/TLS handshake did not complete.");
                    case Hermes::ConnectionErrorEnum::CertificateError:
                        return std::format_to(ctx.out(), "Certificate error: Peer certificate is invalid or untrusted.");
                    case Hermes::ConnectionErrorEnum::EncryptionFailed:
                        return std::format_to(ctx.out(), "Encryption failed: Failed to encrypt outgoing payload.");
                    case Hermes::ConnectionErrorEnum::DecryptionFailed:
                        return std::format_to(ctx.out(), "Decryption failed: Failed to decrypt incoming payload.");
                    case Hermes::ConnectionErrorEnum::InvalidSecurityContext:
                        return std::format_to(ctx.out(), "Invalid security context: Cryptographic context missing or corrupted.");
                    case Hermes::ConnectionErrorEnum::IncompleteMessage:
                        return std::format_to(ctx.out(), "Incomplete message: Received data frame is truncated.");
                    case Hermes::ConnectionErrorEnum::ResolveHostNotFound:
                        return std::format_to(ctx.out(), "Resolve host not found: DNS resolution failed (no such host).");
                    case Hermes::ConnectionErrorEnum::ResolveServiceNotFound:
                        return std::format_to(ctx.out(), "Resolve service not found: Service name or port unrecognized.");
                    case Hermes::ConnectionErrorEnum::ResolveTemporaryFailure:
                        return std::format_to(ctx.out(), "Resolve temporary failure: DNS error, try again later.");
                    case Hermes::ConnectionErrorEnum::ResolveFailed:
                        return std::format_to(ctx.out(), "Resolve failed: General failure during name resolution.");
                    case Hermes::ConnectionErrorEnum::ResolveNoAddressFound:
                        return std::format_to(ctx.out(), "Resolve no address found: Valid host but no IP associated.");
                    case Hermes::ConnectionErrorEnum::UnsupportedAddressFamily:
                        return std::format_to(ctx.out(), "Unsupported address family: Address family not supported.");
                }
            } else {
                switch (error) {
                    case Hermes::ConnectionErrorEnum::Unknown:
                        return std::format_to(ctx.out(), "Unknown error");
                    case Hermes::ConnectionErrorEnum::InvalidRole:
                        return std::format_to(ctx.out(), "Invalid role");
                    case Hermes::ConnectionErrorEnum::InvalidEndpoint:
                        return std::format_to(ctx.out(), "Invalid endpoint");
                    case Hermes::ConnectionErrorEnum::AddressInUse:
                        return std::format_to(ctx.out(), "Address in use");
                    case Hermes::ConnectionErrorEnum::SocketNotOpen:
                        return std::format_to(ctx.out(), "Socket not open");
                    case Hermes::ConnectionErrorEnum::ConnectionFailed:
                        return std::format_to(ctx.out(), "Connection failed");
                    case Hermes::ConnectionErrorEnum::ConnectionClosed:
                        return std::format_to(ctx.out(), "Connection closed");
                    case Hermes::ConnectionErrorEnum::Interrupted:
                        return std::format_to(ctx.out(), "Interrupted");
                    case Hermes::ConnectionErrorEnum::ConnectionTimeout:
                        return std::format_to(ctx.out(), "Connection timeout");
                    case Hermes::ConnectionErrorEnum::SendTimeout:
                        return std::format_to(ctx.out(), "Send timeout");
                    case Hermes::ConnectionErrorEnum::ReceiveTimeout:
                        return std::format_to(ctx.out(), "Receive timeout");
                    case Hermes::ConnectionErrorEnum::ListenFailed:
                        return std::format_to(ctx.out(), "Listen failed");
                    case Hermes::ConnectionErrorEnum::SendFailed:
                        return std::format_to(ctx.out(), "Send failed");
                    case Hermes::ConnectionErrorEnum::ReceiveFailed:
                        return std::format_to(ctx.out(), "Receive failed");
                    case Hermes::ConnectionErrorEnum::HandshakeFailed:
                        return std::format_to(ctx.out(), "Handshake failed");
                    case Hermes::ConnectionErrorEnum::CertificateError:
                        return std::format_to(ctx.out(), "Certificate error");
                    case Hermes::ConnectionErrorEnum::EncryptionFailed:
                        return std::format_to(ctx.out(), "Encryption failed");
                    case Hermes::ConnectionErrorEnum::DecryptionFailed:
                        return std::format_to(ctx.out(), "Decryption failed");
                    case Hermes::ConnectionErrorEnum::InvalidSecurityContext:
                        return std::format_to(ctx.out(), "Invalid security context");
                    case Hermes::ConnectionErrorEnum::IncompleteMessage:
                        return std::format_to(ctx.out(), "Incomplete message");
                    case Hermes::ConnectionErrorEnum::ResolveHostNotFound:
                        return std::format_to(ctx.out(), "Resolve host not found");
                    case Hermes::ConnectionErrorEnum::ResolveServiceNotFound:
                        return std::format_to(ctx.out(), "Resolve service not found");
                    case Hermes::ConnectionErrorEnum::ResolveTemporaryFailure:
                        return std::format_to(ctx.out(), "Resolve temporary failure");
                    case Hermes::ConnectionErrorEnum::ResolveFailed:
                        return std::format_to(ctx.out(), "Resolve failed");
                    case Hermes::ConnectionErrorEnum::ResolveNoAddressFound:
                        return std::format_to(ctx.out(), "Resolve no address found");
                    case Hermes::ConnectionErrorEnum::UnsupportedAddressFamily:
                        return std::format_to(ctx.out(), "Unsupported address family");
                }
            }

            return std::format_to(ctx.out(), "Unknown");
        }
    };
}
