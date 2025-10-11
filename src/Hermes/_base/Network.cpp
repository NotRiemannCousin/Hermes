#include <Hermes/_base/Network.hpp>
#include <Hermes/_base/WinAPI.hpp>
#include <stdexcept>


namespace Hermes {
    WSADATA Network::_wsaData{};
    bool Network::_initialized{};

    CredHandle Network::_credHandle{};
    SCHANNEL_CRED Network::_credData{};
    TimeStamp Network::_tsExpiry{};


    void Network::Initialize() {
        if (_initialized)
            return;

        const int result{ WSAStartup(macroWINSOCK_VERSION, &_wsaData) };
        if (result != 0)
            throw std::runtime_error("WSAStartup failed");

        _credData.dwVersion = static_cast<DWORD>(SCHCredEnum::SCHANNEL);
        _credData.grbitEnabledProtocols = static_cast<DWORD>(SupportedProtocolsEnum::ALL);

        // | Keep in mind to keep macroUNISP_NAME null-terminated.
        const long status = AcquireCredentialsHandleA(
            nullptr,                                                   // Principal
            const_cast<LPSTR>(macroUNISP_NAME.data()),                 // Host
            static_cast<unsigned long>(CredentialFlags::OUTBOUND),     // Client
            nullptr,
            &_credData,
            nullptr, nullptr,
            &_credHandle,
            &_tsExpiry);

        if (status != 0) {
            Cleanup();
            throw std::runtime_error("AcquireCredentialsHandle failed");
        }

        _initialized = true;
    }

    void Network::Cleanup() {
        if (!_initialized)
            return;

        WSACleanup();
        FreeCredentialsHandle(&_credHandle);

        _initialized = false;
    }
    const CredHandle &Network::GetCredHandle() {
        return _credHandle;
    }

    inline auto ____init____ = [] {
        Network::Initialize();
        return 0;
    }();
} // namespace Hermes
