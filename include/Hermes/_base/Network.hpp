#pragma once
#include <Hermes/_base/Credentials.hpp>
#include <Hermes/Config.hpp>


namespace Hermes {
    //! Network is a static utility class used to initialize and access important things like WSA stuff and
    //! credentials.
    struct Network {
        Network() = delete;

        static void Initialize();
#if HERMES_ENABLE_TLS
        static const Credentials& GetClientCredentials();
#endif
    };
}
