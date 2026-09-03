#include <stdexcept>
#include <Hermes/Config.hpp>
#include <Hermes/_base/Network.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
// TODO: FUTURE: Remove and link
#pragma comment(lib, "Ws2_32.lib")
#if HERMES_ENABLE_TLS
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")
#endif
#else
#if HERMES_ENABLE_TLS
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif
#endif

namespace {
#ifdef _WIN32
    struct WsaLifecycle {
        WsaLifecycle() {
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
                throw std::runtime_error{ "WSAStartup failed" };
        }

        ~WsaLifecycle() {
            WSACleanup();
        }
    };

    using AuthServerLifecycle = WsaLifecycle;
#else

#if HERMES_ENABLE_TLS
    struct OpenSllLifecycle {
        OpenSllLifecycle() {
            // OPENSSL_init_ssl is idempotent (1.1+) and threads-safe; loads
            // default config + error strings so OpenSSL is ready to use.
            if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS
                                 | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr) != 1)
                throw std::runtime_error{ "OPENSSL_init_ssl failed" };
        }
    };

    using AuthServerLifecycle = OpenSllLifecycle;
#else
    struct EmptyLifecycle {};
    using AuthServerLifecycle = EmptyLifecycle;
#endif

#endif
}
namespace Hermes {
    void Network::Initialize() {
        [[maybe_unused]] static AuthServerLifecycle globalAuth;
    }

#if HERMES_ENABLE_TLS
    const Credentials& Network::GetClientCredentials() {
        Initialize();
        thread_local Credentials credentials{ Credentials::Client().value() };

        return credentials;
    }
#endif
}