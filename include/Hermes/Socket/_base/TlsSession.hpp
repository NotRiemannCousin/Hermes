#pragma once
#include <Hermes/_base/OsApi/OsApi.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace Hermes {
    struct Credentials;
}

namespace Hermes::_details {

    //! @brief Platform-agnostic TLS session — wraps SCHANNEL on Windows, OpenSSL on Linux.
    //!
    //! State machines orchestrate I/O (recv/send) around this session; the session
    //! owns the underlying TLS context and exposes only byte-level operations.
    //! Status codes follow the existing EncryptStatusEnum so the switch logic in
    //! the state machines is identical on both platforms.
    struct TlsSession {
        struct StreamSizes {
            std::uint32_t header{};
            std::uint32_t trailer{};
            std::uint32_t maxMessage{};
        };

        struct HandshakeOutcome {
            EncryptStatusEnum status{};
            std::uint32_t     consumed{}; // bytes consumed from inBytes
            std::uint32_t     produced{}; // bytes written into outBuf
        };

        struct EncryptOutcome {
            EncryptStatusEnum status{};
            std::uint32_t     produced{};
        };

        struct DecryptOutcome {
            EncryptStatusEnum    status{};
            std::span<std::byte> data{};  // decrypted payload (in-place on Windows; session-owned on Linux)
            std::span<std::byte> extra{}; // unconsumed ciphertext belonging to the next record
        };


        TlsSession();
        ~TlsSession();

        TlsSession(const TlsSession&)            = delete;
        TlsSession& operator=(const TlsSession&) = delete;

        TlsSession(TlsSession&& other) noexcept;
        TlsSession& operator=(TlsSession&& other) noexcept;


        //! Initialize as a client. Calling Begin* on an already-active session is
        //! treated as a renegotiation on the existing context.
        void BeginClient(const Credentials& creds, std::string_view host,
                         bool ignoreCertErrors, bool mutualAuth) noexcept;

        void BeginServer(const Credentials& creds, bool requestClientCert) noexcept;


        bool IsActive()            const noexcept;
        bool IsHandshakeComplete() const noexcept;
        bool IsRenegotiation()     const noexcept;

        //! Drive one handshake step. May consume part/all of inBytes; may produce
        //! zero or more bytes in outBuf. Caller is responsible for sending the
        //! produced bytes to the peer between calls.
        HandshakeOutcome AdvanceHandshake(
            std::span<std::byte> inBytes,
            std::span<std::byte> outBuf
        ) noexcept;

        //! Valid only after IsHandshakeComplete() is true.
        StreamSizes GetStreamSizes() const noexcept;

        //! Encrypt one record. outBuf must be at least
        //! `GetStreamSizes().header + plain.size() + GetStreamSizes().trailer` bytes.
        EncryptOutcome Encrypt(
            std::span<const std::byte> plain,
            std::span<std::byte>       outBuf
        ) noexcept;

        //! Decrypt from inBytes. `data` is the decrypted payload, `extra` is any
        //! trailing ciphertext that belongs to the next record.
        DecryptOutcome Decrypt(std::span<std::byte> inBytes) noexcept;

        //! Initiate close_notify. Returns the byte count written to outBuf.
        std::uint32_t Shutdown(std::span<std::byte> outBuf) noexcept;

        //! Tear down the TLS context; session becomes inactive but the object remains valid.
        void DeleteContext() noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };

}
