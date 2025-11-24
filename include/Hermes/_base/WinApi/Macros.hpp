#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define SECURITY_WIN32


#include <string_view>
#include <winsock2.h>
#include <schannel.h>
#include <sspi.h>

constexpr size_t macroWINSOCK_VERSION{ WINSOCK_VERSION };

constexpr size_t macroSG_UNCONSTRAINED_GROUP { SG_UNCONSTRAINED_GROUP };
constexpr size_t macroSG_CONSTRAINED_GROUP   { SG_CONSTRAINED_GROUP };

constexpr size_t macroSO_TYPE          { SO_TYPE };
constexpr size_t macroSOL_SOCKET       { SOL_SOCKET };
constexpr size_t macroSO_PROTOCOL_INFO { SO_PROTOCOL_INFO };


// ----------------------------------------------------------------------------------------------------
//                             Error Codes
// ----------------------------------------------------------------------------------------------------


constexpr SOCKET macroINVALID_SOCKET { INVALID_SOCKET };
constexpr size_t macroSOCKET_ERROR   { static_cast<size_t>(SOCKET_ERROR) };


// ----------------------------------------------------------------------------------------------------
//                             SSL and TLS
// ----------------------------------------------------------------------------------------------------


constexpr std::string_view macroUNISP_NAME{ UNISP_NAME };


// ----------------------------------------------------------------------------------------------------
//                             Utils
// ----------------------------------------------------------------------------------------------------


constexpr unsigned long macroSECBUFFER_VERSION        { SECBUFFER_VERSION };
constexpr unsigned long macroSECPKG_ATTR_STREAM_SIZES { SECPKG_ATTR_STREAM_SIZES };


constexpr bool macroSUCCEEDED(auto &&d) { return SUCCEEDED(d); }

constexpr bool macroFAILED(auto &&d) { return FAILED(d); }