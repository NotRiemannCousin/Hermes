#pragma once
#include <Hermes/_base/OsApi/OsApi.hpp>

namespace Hermes {
    //! @brief Context structure for asynchronous I/O completion callbacks.
    struct TransferOperStatus;

#ifdef _WIN32
    struct TransferOperStatus : WSAOVERLAPPED {
        using Operation = void(void* context, LongIoCount transferedBytes, bool success);

        void* context{};
        Operation* callback{};
    };
#else
    struct TransferOperStatus {
        using Operation = void(void* context, size_t transferedBytes, bool success);

        void* context{};
        Operation* callback{};
    };
#endif
}