#pragma once
// This file it's a wrapper to hold the things from windows headers
// used to manage socket connections and other things. Almost every
// code here it's just the export of windows headers to C++20 modules.
// (and that it's why it's a mess

// I cant forward macros so some things are wrapped in enums, constexpr
// with "macro" prefix and similarities. There is also some utilities
// functions to make things easier.

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define SECURITY_WIN32

#include <winsock2.h>
#include <schannel.h>
#include <ws2tcpip.h>
#include <sspi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")

#pragma region Macro Operations
// template<class E> requires std::is_scoped_enum_v<E>
// constexpr bool IsEnumFlag(){
//     return std::string_view(__FUNCSIG__).ends_with("Flags>(void)");
// }
//
// template<class E>
// concept EnumFlagConcept = requires { requires IsEnumFlag<E>(); };


#define FLAGS_OPERATIONS(TYPE)                                                              \
        static_assert(std::is_enum_v<TYPE>);                                                \
        constexpr TYPE operator|(TYPE f1, TYPE f2) {                                        \
            using type = std::underlying_type_t<decltype(f1)>;                              \
            return static_cast<TYPE>(type(f1) | type(f2));                                  \
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

// export {
#include <Hermes/_base/WinApi/ConversionAndLiterals.hpp>
#include <Hermes/_base/WinApi/Macros.hpp>

#include <Hermes/_base/WinApi/Enums/AddressFamilyEnum.hpp>
#include <Hermes/_base/WinApi/Enums/ConditionFunctionEnum.hpp>
#include <Hermes/_base/WinApi/Enums/EncryptStatusEnum.hpp>
#include <Hermes/_base/WinApi/Enums/ProtocolBaseTypeEnum.hpp>
#include <Hermes/_base/WinApi/Enums/ProtocolTypeEnum.hpp>
#include <Hermes/_base/WinApi/Enums/SChCredEnum.hpp>
#include <Hermes/_base/WinApi/Enums/SecurityBufferEnum.hpp>
#include <Hermes/_base/WinApi/Enums/SocketShutdownEnum.hpp>
#include <Hermes/_base/WinApi/Enums/SocketTypeEnum.hpp>

	ENUM_OPERATIONS(AddressFamilyEnum)
	ENUM_OPERATIONS(ConditionFunctionEnum)
	ENUM_OPERATIONS(EncryptStatusEnum)
	ENUM_OPERATIONS(ProtocolBaseTypeEnum)
	ENUM_OPERATIONS(ProtocolTypeEnum)
	ENUM_OPERATIONS(SChCredEnum)
	ENUM_OPERATIONS(SecurityBufferEnum)
	ENUM_OPERATIONS(SocketShutdownEnum)
    ENUM_OPERATIONS(SocketTypeEnum)


#include <Hermes/_base/WinApi/Flags/CredentialFlags.hpp>
#include <Hermes/_base/WinApi/Flags/SupportedProtocolsFlags.hpp>
#include <Hermes/_base/WinApi/Flags/InitializeSecurityContextFlags.hpp>
#include <Hermes/_base/WinApi/Flags/InitializeSecurityContextReturnFlags.hpp>
#include <Hermes/_base/WinApi/Flags/SchannelCredFlags.hpp>

    FLAGS_OPERATIONS(CredentialFlags)
    FLAGS_OPERATIONS(SupportedProtocolsFlags)
    FLAGS_OPERATIONS(InitializeSecurityContextFlags)
    FLAGS_OPERATIONS(InitializeSecurityContextReturnFlags)
    FLAGS_OPERATIONS(SchannelCredFlags)

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
// };

#undef FLAGS_OPERATIONS
#undef ENUM_OPERATIONS