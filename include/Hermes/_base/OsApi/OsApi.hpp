#pragma once
// This file it's a wrapper to hold the things from the OS headers
// used to manage socket connections and other things. Almost every
// code here it's just the export of OS headers to C++20 modules.
// (and that it's why it's a mess

// I cant forward macros so some things are wrapped in enums, constexpr
// with "macro" prefix and similarities. There is also some utilities
// functions to make things easier.


#pragma region Macro Operations
#define FLAGS_OPERATIONS(TYPE)                                                              \
        static_assert(std::is_enum_v<TYPE>);                                                \
        constexpr TYPE operator|(TYPE f1, TYPE f2) noexcept {                               \
            using type = std::underlying_type_t<decltype(f1)>;                              \
            return static_cast<TYPE>(type(f1) | type(f2));                                  \
        }                                                                                   \
        constexpr TYPE& operator|=(TYPE& f1, TYPE f2) noexcept {                            \
            using T = std::underlying_type_t<TYPE>;                                         \
            f1 = static_cast<TYPE>(static_cast<T>(f1) | static_cast<T>(f2));                \
            return f1;                                                                      \
        }                                                                                   \
                                                                                                \
        constexpr bool operator!=(auto a, TYPE b) noexcept {                                \
            return a != static_cast<std::underlying_type_t<decltype(b)>>(b);                \
        }                                                                                   \
        constexpr bool operator==(auto a, TYPE b) noexcept {                                \
            return a == static_cast<std::underlying_type_t<decltype(b)>>(b);                \
        }

#define ENUM_OPERATIONS(TYPE)                                                               \
        constexpr bool operator!=(auto a, TYPE b) noexcept {                                \
            return a != static_cast<std::underlying_type_t<decltype(b)>>(b);                \
        }                                                                                   \
        constexpr bool operator==(auto a, TYPE b) noexcept {                                \
            return a == static_cast<std::underlying_type_t<decltype(b)>>(b);                \
        }
#pragma endregion

#ifdef _WIN32
#define SCHANNEL_USE_BLACKLISTS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define SECURITY_WIN32


// ReSharper disable once CppUnusedIncludeDirective
#include <ws2tcpip.h>
#include <winsock2.h>

#include <ntdef.h>

// typedef struct _UNICODE_STRING {
//     USHORT Length;
//     USHORT MaximumLength;
//     PWSTR Buffer;
// } UNICODE_STRING, *PUNICODE_STRING;

#include <schannel.h>
#include <sspi.h>

#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif


#ifdef _WIN32
using SocketFd = SOCKET;
using IoCount = int;
using SocketHandle = HANDLE;
#else
using SocketFd = int;
using IoCount = ssize_t;
using SocketHandle = int*;
#endif



int CloseSocket(SocketFd socket);
#ifdef _WIN32
inline int CloseSocket(SocketFd socket) {
    return closesocket(socket);
}
#else
inline int CloseSocket(SocketFd socket) {
    return close(socket);
}
#endif

// export {
// ReSharper disable once CppUnusedIncludeDirective
#include <Hermes/_base/OsApi/ConversionAndLiterals.hpp>
#include <Hermes/_base/OsApi/Macros.hpp>

#include <Hermes/_base/OsApi/Enums/AddressFamilyEnum.hpp>
#include <Hermes/_base/OsApi/Enums/ProtocolBaseTypeEnum.hpp>
#include <Hermes/_base/OsApi/Enums/SocketShutdownEnum.hpp>
#include <Hermes/_base/OsApi/Enums/SocketTypeEnum.hpp>


#ifdef _WIN32
#include <Hermes/_base/OsApi/Enums/Windows/ConditionFunctionEnum.hpp>
#include <Hermes/_base/OsApi/Enums/Windows/EncryptStatusEnum.hpp>
#include <Hermes/_base/OsApi/Enums/Windows/SChCredEnum.hpp>
#include <Hermes/_base/OsApi/Enums/Windows/SecurityBufferEnum.hpp>
#else
#include <Hermes/_base/OsApi/Enums/Linux/EncryptStatusEnum.hpp>
#endif

	ENUM_OPERATIONS(AddressFamilyEnum)
	ENUM_OPERATIONS(ProtocolBaseTypeEnum)
	ENUM_OPERATIONS(SocketShutdownEnum)
    ENUM_OPERATIONS(SocketTypeEnum)

#ifdef _WIN32
	ENUM_OPERATIONS(ConditionFunctionEnum)
	ENUM_OPERATIONS(EncryptStatusEnum)
	ENUM_OPERATIONS(SChCredEnum)
	ENUM_OPERATIONS(SecurityBufferEnum)
#else
	ENUM_OPERATIONS(EncryptStatusEnum)
#endif


#include <Hermes/_base/OsApi/Flags/NameInfoFlags.hpp>

#ifdef _WIN32
#include <Hermes/_base/OsApi/Flags/Windows/CredentialFlags.hpp>
#include <Hermes/_base/OsApi/Flags/Windows/SupportedProtocolsFlags.hpp>
#include <Hermes/_base/OsApi/Flags/Windows/InitializeSecurityContextFlags.hpp>
#include <Hermes/_base/OsApi/Flags/Windows/InitializeSecurityContextReturnFlags.hpp>
#include <Hermes/_base/OsApi/Flags/Windows/AcceptSecurityContextFlags.hpp>
#include <Hermes/_base/OsApi/Flags/Windows/AcceptSecurityContextReturnFlags.hpp>
#include <Hermes/_base/OsApi/Flags/Windows/SChannelCredFlags.hpp>
#else
#include <Hermes/_base/OsApi/Flags/Linux/CredentialFlags.hpp>
#include <Hermes/_base/OsApi/Flags/Linux/SupportedProtocolsFlags.hpp>
#include <Hermes/_base/OsApi/Flags/Linux/SChannelCredFlags.hpp>
#endif

#ifdef _WIN32
    FLAGS_OPERATIONS(CredentialFlags)
    FLAGS_OPERATIONS(SupportedProtocolsFlags)
    FLAGS_OPERATIONS(InitializeSecurityContextFlags)
    FLAGS_OPERATIONS(InitializeSecurityContextReturnFlags)
    FLAGS_OPERATIONS(AcceptSecurityContextFlags)
    FLAGS_OPERATIONS(AcceptSecurityContextReturnFlags)
    FLAGS_OPERATIONS(SChannelCredFlags)

    using ::CredHandle;
    using ::CtxtHandle;
    using ::SCHANNEL_CRED;
    using ::TimeStamp;


    using ::SecBuffer;
    using ::SecBufferDesc;

    using ::SECURITY_STATUS;

    using ::AcquireCredentialsHandleA;
    using ::AcquireCredentialsHandleW;
    using ::FreeCredentialsHandle;


    using ::InitializeSecurityContextA;
    using ::InitializeSecurityContextW;
    using ::CompleteAuthToken;


    using ::EncryptMessage;
    using ::DecryptMessage;

    using ::QueryContextAttributesA;
    using ::QueryContextAttributesW;
    using ::SecPkgContext_StreamSizes;
#else
    FLAGS_OPERATIONS(CredentialFlags)
    FLAGS_OPERATIONS(SupportedProtocolsFlags)
    FLAGS_OPERATIONS(SChannelCredFlags)
#endif
// };

#undef FLAGS_OPERATIONS
#undef ENUM_OPERATIONS