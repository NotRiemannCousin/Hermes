#pragma once
#include <Hermes/_base/Credentials.hpp>


namespace Hermes {
    //! Network is a static utility class used to initializing and access important things like WSA stuff and
    //! credentials.
    struct Network {
        Network() = delete;

        static void Initialize();
        static const Credentials& GetClientCredentials();
    };
}
