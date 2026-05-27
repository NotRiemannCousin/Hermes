#include <Hermes/_base/Network.hpp>
#include <Hermes/_base/OsApi/OsApi.hpp>
#include <stdexcept>

#ifdef _WIN32
// TODO: FUTURE: Remove and link
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")
#else
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

namespace Hermes {
    namespace Detail {
#ifdef _WIN32
        struct WsaLifecycle {
            WsaLifecycle() {
                WSADATA wsaData;
                if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
                    throw std::runtime_error("WSAStartup failed");
            }

            ~WsaLifecycle() {
                WSACleanup();
            }
        };
#else
        struct OpenSSLLifecycle {
            OpenSSLLifecycle() {
                // OPENSSL_init_ssl is idempotent (1.1+) and threads-safe; loads
                // default config + error strings so OpenSSL is ready to use.
                if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS
                                   | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr) != 1)
                    throw std::runtime_error("OPENSSL_init_ssl failed");
            }
        };
#endif
    }

    void Network::Initialize() {
#ifdef _WIN32
        static Detail::WsaLifecycle globalWsa;
#else
        static Detail::OpenSSLLifecycle globalSsl;
#endif
    }

    const Credentials& Network::GetClientCredentials() {
        Initialize();
        thread_local Credentials credentials{ Credentials::Client().value() };

        return credentials;
    }

}
