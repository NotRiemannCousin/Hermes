#pragma once
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base/Data/TlsSocketData.hpp>
#include <Hermes/Socket/_base/Accept/DefaultAcceptPolicy.hpp>
#include <Hermes/Socket/_base/_base.hpp>

namespace Hermes {

    template<SocketDataConcept Data = TlsSocketData<>>
    struct TlsAcceptPolicy {
        struct ListenOptions : DefaultAcceptPolicy<Data>::ListenOptions {
            // std::vector<std::string> alpnProtocols;
            
            bool requireValidClientCertificate{ true };
        };
        struct AcceptOptions : DefaultAcceptPolicy<Data>::AcceptOptions {
            std::chrono::milliseconds handshakeTimeout{};
            bool requestClientCertificate{};
        };


        //! @brief Creates the listening socket: socket() + bind() + listen().
        //! Identical to DefaultAcceptPolicy::Listen — TLS operates per-connection,
        //! not on the listening socket itself.
        static ConnectionResultOper Listen(Data& data, int backlog, ListenOptions options) noexcept;

        //! @brief Accepts one incoming connection and performs the TLS server handshake.
        //! Fills outData.socket, outData.endpoint, and all SChannel context fields.
        //! Sets outData.isServer = true on success.
        static ConnectionResultOper Accept(Data& data, Data& outData, AcceptOptions options) noexcept;

        //! @brief Sends the TLS close_notify alert and closes the accepted connection.
        static void Close(Data& data) noexcept;

        //! @brief Closes the listening socket. No TLS alert needed.
        static void Abort(Data& data) noexcept;

    private:
        static ConnectionResultOper ServerHandshake(Data& data, AcceptOptions options) noexcept;
    };

}

#include <Hermes/Socket/_base/Accept/TlsAcceptPolicy.tpp>

namespace Hermes {
    static_assert(AcceptPolicyConcept<TlsAcceptPolicy, TlsSocketData<>>);
}