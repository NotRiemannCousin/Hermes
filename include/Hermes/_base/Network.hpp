#pragma once
#include <Hermes/_base/WinApi.hpp>


namespace Hermes {
    //! Network is a static utility class used to initializing and access important things like WSA stuff and
    //! credentials.
    class Network {
    public:
        static void Initialize();
        static void Cleanup();

        Network() = delete;

        static const CredHandle& GetCredHandle();
    private:
        static WSADATA _wsaData;
        static bool _initialized;

        static CredHandle _credHandle;
        static SCHANNEL_CRED _credData;
        static TimeStamp _tsExpiry;
    };
} // namespace Hermes
