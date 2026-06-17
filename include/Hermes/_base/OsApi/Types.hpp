#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#endif

namespace Hermes {
#ifdef _WIN32
    using SocketFd = SOCKET;
    using IoCount = int;
    using LongIoCount = DWORD;
    using SocketHandle = HANDLE;
#else
    using SocketFd = int;
    using IoCount = ssize_t;
    using LongIoCount = unsigned long;
    using SocketHandle = int*;
#endif
}