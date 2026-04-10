#include <Hermes/_base/Network.hpp>
#include <Hermes/_base/WinApi/WinApi.hpp>
#include <stdexcept>

// TODO: FUTURE: Remove and link
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")

namespace Hermes {
    namespace Detail {
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
    }

    void Network::Initialize() {
        static Detail::WsaLifecycle globalWsa;
    }

    const Credentials& Network::GetClientCredentials() {
        Initialize();
        thread_local Credentials credentials{ Credentials::Client().value() };

        return credentials;
    }

}